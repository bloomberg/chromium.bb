// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_server.h"

#include <pwd.h>
#include <cstring>

#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "grit/browser_resources.h"
#include "jni/DevToolsServer_jni.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_listen_socket_posix.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

// TL;DR: Do not change this string.
//
// Desktop Chrome relies on this format to identify debuggable apps on Android
// (see the code under chrome/browser/devtools/device).
// If this string ever changes it would not be sufficient to change the
// corresponding string on the client side. Since debugging an older version of
// Chrome for Android from a newer version of desktop Chrome is a very common
// scenario, the client code will have to be modified to recognize both the old
// and the new format.
const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/devtools.html";
const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

bool AuthorizeSocketAccessWithDebugPermission(
    const net::UnixDomainServerSocket::Credentials& credentials) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DevToolsServer_checkDebugPermission(
      env, base::android::GetApplicationContext(),
      credentials.process_id, credentials.user_id) ||
      content::CanUserConnectToDevTools(credentials);
}

// Delegate implementation for the devtools http handler on android. A new
// instance of this gets created each time devtools is enabled.
class DevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  explicit DevToolsServerDelegate(
      const net::UnixDomainServerSocket::AuthCallback& auth_callback)
      : last_tethering_socket_(0),
        auth_callback_(auth_callback) {
  }

  virtual std::string GetDiscoveryPageHTML() OVERRIDE {
    // TopSites updates itself after a delay. Ask TopSites to update itself
    // when we're about to show the remote debugging landing page.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsServerDelegate::PopulatePageThumbnails));
    return ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
  }

  virtual bool BundlesFrontendResources() OVERRIDE {
    return false;
  }

  virtual base::FilePath GetDebugFrontendDir() OVERRIDE {
    return base::FilePath();
  }

  virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(
      net::StreamListenSocket::Delegate* delegate,
      std::string* name) OVERRIDE {
    *name = base::StringPrintf(
        kTetheringSocketName, getpid(), ++last_tethering_socket_);
    return net::deprecated::UnixDomainListenSocket::
        CreateAndListenWithAbstractNamespace(
            *name,
            "",
            delegate,
            auth_callback_)
        .PassAs<net::StreamListenSocket>();
  }

 private:
  static void PopulatePageThumbnails() {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (top_sites)
      top_sites->SyncWithHistory();
  }

  int last_tethering_socket_;
  const net::UnixDomainServerSocket::AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerDelegate);
};

// Factory for UnixDomainServerSocket. It tries a fallback socket when
// original socket doesn't work.
class UnixDomainServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  UnixDomainServerSocketFactory(
      const std::string& socket_name,
      const net::UnixDomainServerSocket::AuthCallback& auth_callback)
      : content::DevToolsHttpHandler::ServerSocketFactory(socket_name, 0, 1),
        auth_callback_(auth_callback) {
  }

 private:
  // content::DevToolsHttpHandler::ServerSocketFactory.
  virtual scoped_ptr<net::ServerSocket> Create() const OVERRIDE {
    return scoped_ptr<net::ServerSocket>(
        new net::UnixDomainServerSocket(auth_callback_,
                                        true /* use_abstract_namespace */));
  }

  virtual scoped_ptr<net::ServerSocket> CreateAndListen() const OVERRIDE {
    scoped_ptr<net::ServerSocket> socket = Create();
    if (!socket)
      return scoped_ptr<net::ServerSocket>();

    if (socket->ListenWithAddressAndPort(address_, port_, backlog_) == net::OK)
      return socket.Pass();

    // Try a fallback socket name.
    const std::string fallback_address(
        base::StringPrintf("%s_%d", address_.c_str(), getpid()));
    if (socket->ListenWithAddressAndPort(fallback_address, port_, backlog_)
        == net::OK)
      return socket.Pass();

    return scoped_ptr<net::ServerSocket>();
  }

  const net::UnixDomainServerSocket::AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

DevToolsServer::DevToolsServer(const std::string& socket_name_prefix)
    : socket_name_(base::StringPrintf(kDevToolsChannelNameFormat,
                                      socket_name_prefix.c_str())),
      protocol_handler_(NULL) {
  // Override the socket name if one is specified on the command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name_ = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
}

DevToolsServer::~DevToolsServer() {
  Stop();
}

void DevToolsServer::Start(bool allow_debug_permission) {
  if (protocol_handler_)
    return;

  net::UnixDomainServerSocket::AuthCallback auth_callback =
      allow_debug_permission ?
          base::Bind(&AuthorizeSocketAccessWithDebugPermission) :
          base::Bind(&content::CanUserConnectToDevTools);
  scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory> factory(
      new UnixDomainServerSocketFactory(socket_name_, auth_callback));
  protocol_handler_ = content::DevToolsHttpHandler::Start(
      factory.Pass(),
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      new DevToolsServerDelegate(auth_callback),
      base::FilePath());
}

void DevToolsServer::Stop() {
  if (!protocol_handler_)
    return;
  // Note that the call to Stop() below takes care of |protocol_handler_|
  // deletion.
  protocol_handler_->Stop();
  protocol_handler_ = NULL;
}

bool DevToolsServer::IsStarted() const {
  return protocol_handler_;
}

bool RegisterDevToolsServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong InitRemoteDebugging(JNIEnv* env,
                                jobject obj,
                                jstring socket_name_prefix) {
  DevToolsServer* server = new DevToolsServer(
      base::android::ConvertJavaStringToUTF8(env, socket_name_prefix));
  return reinterpret_cast<intptr_t>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env, jobject obj, jlong server) {
  delete reinterpret_cast<DevToolsServer*>(server);
}

static jboolean IsRemoteDebuggingEnabled(JNIEnv* env,
                                         jobject obj,
                                         jlong server) {
  return reinterpret_cast<DevToolsServer*>(server)->IsStarted();
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      jobject obj,
                                      jlong server,
                                      jboolean enabled,
                                      jboolean allow_debug_permission) {
  DevToolsServer* devtools_server = reinterpret_cast<DevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start(allow_debug_permission);
  } else {
    devtools_server->Stop();
  }
}
