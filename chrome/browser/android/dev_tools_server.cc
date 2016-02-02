// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_server.h"

#include <pwd.h>
#include <cstring>
#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_content_client.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "components/history/core/browser/top_sites.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
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
#include "net/socket/unix_domain_server_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;
using devtools_http_handler::DevToolsHttpHandler;

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
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

const int kBackLog = 10;

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
class DevToolsServerDelegate :
    public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  DevToolsServerDelegate() {
  }

  std::string GetDiscoveryPageHTML() override {
    // TopSites updates itself after a delay. Ask TopSites to update itself
    // when we're about to show the remote debugging landing page.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsServerDelegate::PopulatePageThumbnails));
    return ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
  }

  std::string GetFrontendResource(const std::string& path) override {
    return std::string();
  }

  std::string GetPageThumbnailData(const GURL& url) override {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile);
    if (top_sites) {
      scoped_refptr<base::RefCountedMemory> data;
      if (top_sites->GetPageThumbnail(url, false, &data))
        return std::string(data->front_as<char>(), data->size());
    }
    return std::string();
  }

  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override {
    return nullptr;
  }

 private:
  static void PopulatePageThumbnails() {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile);
    if (top_sites)
      top_sites->SyncWithHistory();
  }

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerDelegate);
};

// Factory for UnixDomainServerSocket. It tries a fallback socket when
// original socket doesn't work.
class UnixDomainServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  UnixDomainServerSocketFactory(
      const std::string& socket_name,
      const net::UnixDomainServerSocket::AuthCallback& auth_callback)
      : socket_name_(socket_name),
        last_tethering_socket_(0),
        auth_callback_(auth_callback) {
  }

 private:
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(auth_callback_,
                                        true /* use_abstract_namespace */));

    if (socket->BindAndListen(socket_name_, kBackLog) == net::OK)
      return std::move(socket);

    // Try a fallback socket name.
    const std::string fallback_address(
        base::StringPrintf("%s_%d", socket_name_.c_str(), getpid()));
    if (socket->BindAndListen(fallback_address, kBackLog) == net::OK)
      return std::move(socket);

    return scoped_ptr<net::ServerSocket>();
  }

  scoped_ptr<net::ServerSocket> CreateForTethering(std::string* name) override {
    *name = base::StringPrintf(
        kTetheringSocketName, getpid(), ++last_tethering_socket_);
    scoped_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(auth_callback_, true));
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_;
  net::UnixDomainServerSocket::AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

DevToolsServer::DevToolsServer(const std::string& socket_name_prefix)
    : socket_name_(base::StringPrintf(kDevToolsChannelNameFormat,
                                      socket_name_prefix.c_str())) {
  // Override the socket name if one is specified on the command line.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name_ = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
}

DevToolsServer::~DevToolsServer() {
  Stop();
}

void DevToolsServer::Start(bool allow_debug_permission) {
  if (devtools_http_handler_)
    return;

  net::UnixDomainServerSocket::AuthCallback auth_callback =
      allow_debug_permission ?
          base::Bind(&AuthorizeSocketAccessWithDebugPermission) :
          base::Bind(&content::CanUserConnectToDevTools);
  scoped_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new UnixDomainServerSocketFactory(socket_name_, auth_callback));
  devtools_http_handler_.reset(new DevToolsHttpHandler(
      std::move(factory),
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      new DevToolsServerDelegate(), base::FilePath(), base::FilePath(),
      version_info::GetProductNameAndVersionForUserAgent(), ::GetUserAgent()));
}

void DevToolsServer::Stop() {
  devtools_http_handler_.reset();
}

bool DevToolsServer::IsStarted() const {
  return devtools_http_handler_;
}

bool RegisterDevToolsServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong InitRemoteDebugging(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& socket_name_prefix) {
  DevToolsServer* server = new DevToolsServer(
      base::android::ConvertJavaStringToUTF8(env, socket_name_prefix));
  return reinterpret_cast<intptr_t>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jlong server) {
  delete reinterpret_cast<DevToolsServer*>(server);
}

static jboolean IsRemoteDebuggingEnabled(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jlong server) {
  return reinterpret_cast<DevToolsServer*>(server)->IsStarted();
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
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
