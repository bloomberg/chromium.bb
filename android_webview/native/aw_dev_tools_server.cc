// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_dev_tools_server.h"

#include "android_webview/native/aw_contents.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "jni/AwDevToolsServer_jni.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"

using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
const char kSocketNameFormat[] = "webview_devtools_remote_%d";
const char kTetheringSocketName[] = "webview_devtools_tethering_%d_%d";

const int kBackLog = 10;

// Delegate implementation for the devtools http handler for WebView. A new
// instance of this gets created each time web debugging is enabled.
class AwDevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  AwDevToolsServerDelegate() {
  }

  ~AwDevToolsServerDelegate() override {}

  // DevToolsHttpProtocolHandler::Delegate overrides.
  std::string GetDiscoveryPageHTML() override;

  bool BundlesFrontendResources() override {
    return false;
  }

  base::FilePath GetDebugFrontendDir() override {
    return base::FilePath();
  }
 private:

  DISALLOW_COPY_AND_ASSIGN(AwDevToolsServerDelegate);
};


std::string AwDevToolsServerDelegate::GetDiscoveryPageHTML() {
  const char html[] =
      "<html>"
      "<head><title>WebView remote debugging</title></head>"
      "<body>Please use <a href=\'chrome://inspect\'>chrome://inspect</a>"
      "</body>"
      "</html>";
  return html;
}

// Factory for UnixDomainServerSocket.
class UnixDomainServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name),
        last_tethering_socket_(0) {
  }

 private:
  // content::DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::ServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->ListenWithAddressAndPort(socket_name_, 0, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket;
  }

  scoped_ptr<net::ServerSocket> CreateForTethering(std::string* name) override {
    *name = base::StringPrintf(
        kTetheringSocketName, getpid(), ++last_tethering_socket_);
    scoped_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools), true));
    if (socket->ListenWithAddressAndPort(*name, 0, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket.Pass();
  }

  std::string socket_name_;
  int last_tethering_socket_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

namespace android_webview {

AwDevToolsServer::AwDevToolsServer() {
}

AwDevToolsServer::~AwDevToolsServer() {
  Stop();
}

void AwDevToolsServer::Start() {
  if (protocol_handler_)
    return;

  scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory> factory(
      new UnixDomainServerSocketFactory(
          base::StringPrintf(kSocketNameFormat, getpid())));
  protocol_handler_.reset(content::DevToolsHttpHandler::Start(
      factory.Pass(),
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      new AwDevToolsServerDelegate(),
      base::FilePath()));
}

void AwDevToolsServer::Stop() {
  protocol_handler_.reset();
}

bool AwDevToolsServer::IsStarted() const {
  return protocol_handler_;
}

bool RegisterAwDevToolsServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong InitRemoteDebugging(JNIEnv* env,
                                jobject obj) {
  AwDevToolsServer* server = new AwDevToolsServer();
  return reinterpret_cast<intptr_t>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env, jobject obj, jlong server) {
  delete reinterpret_cast<AwDevToolsServer*>(server);
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      jobject obj,
                                      jlong server,
                                      jboolean enabled) {
  AwDevToolsServer* devtools_server =
      reinterpret_cast<AwDevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start();
  } else {
    devtools_server->Stop();
  }
}

}  // namespace android_webview
