// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_devtools_server.h"

#include <utility>

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/common/aw_content_client.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "jni/AwDevToolsServer_jni.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"

using base::android::JavaParamRef;
using content::DevToolsAgentHost;

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
const char kSocketNameFormat[] = "webview_devtools_remote_%d";
const char kTetheringSocketName[] = "webview_devtools_tethering_%d_%d";

const int kBackLog = 10;

// Factory for UnixDomainServerSocket.
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name), last_tethering_socket_(0) {}

 private:
  // content::DevToolsAgentHost::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    *name = base::StringPrintf(kTetheringSocketName, getpid(),
                               ++last_tethering_socket_);
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

namespace android_webview {

AwDevToolsServer::AwDevToolsServer() : is_started_(false) {}

AwDevToolsServer::~AwDevToolsServer() {
  Stop();
}

void AwDevToolsServer::Start() {
  if (is_started_)
    return;
  is_started_ = true;

  std::unique_ptr<content::DevToolsSocketFactory> factory(
      new UnixDomainServerSocketFactory(
          base::StringPrintf(kSocketNameFormat, getpid())));
  DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory),
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      base::FilePath(), base::FilePath());
}

void AwDevToolsServer::Stop() {
  DevToolsAgentHost::StopRemoteDebuggingServer();
  is_started_ = false;
}

bool AwDevToolsServer::IsStarted() const {
  return is_started_;
}

static jlong InitRemoteDebugging(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  AwDevToolsServer* server = new AwDevToolsServer();
  return reinterpret_cast<intptr_t>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jlong server) {
  delete reinterpret_cast<AwDevToolsServer*>(server);
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
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
