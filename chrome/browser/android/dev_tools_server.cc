// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_server.h"

#include <cstring>
#include <pwd.h>

#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "jni/DevToolsServer_jni.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/base/unix_domain_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/static/%s/devtools.html";
const char kSocketName[] = "chrome_devtools_remote";

// Delegate implementation for the devtools http handler on android. A new
// instance of this gets created each time devtools is enabled.
class DevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  explicit DevToolsServerDelegate(bool use_bundled_frontend_resources)
      : use_bundled_frontend_resources_(use_bundled_frontend_resources) {
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
    return use_bundled_frontend_resources_;
  }

  virtual base::FilePath GetDebugFrontendDir() OVERRIDE {
    return base::FilePath();
  }

  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (top_sites) {
      scoped_refptr<base::RefCountedMemory> data;
      if (top_sites->GetPageThumbnail(url, &data))
        return std::string(reinterpret_cast<const char*>(data->front()),
                           data->size());
    }
    return "";
  }

  virtual content::RenderViewHost* CreateNewTarget() OVERRIDE {
    return NULL;
  }

  virtual TargetType GetTargetType(content::RenderViewHost*) OVERRIDE {
    return kTargetTypeTab;
  }

  virtual std::string GetViewDescription(content::RenderViewHost*) OVERRIDE {
    return "";
  }

 private:
  static void PopulatePageThumbnails() {
    Profile* profile =
        ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (top_sites)
      top_sites->SyncWithHistory();
  }

  bool use_bundled_frontend_resources_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerDelegate);
};

}  // namespace

DevToolsServer::DevToolsServer()
    : use_bundled_frontend_resources_(false),
      socket_name_(kSocketName),
      protocol_handler_(NULL) {
}

DevToolsServer::DevToolsServer(bool use_bundled_frontend_resources,
                               const std::string& socket_name)
    : use_bundled_frontend_resources_(use_bundled_frontend_resources),
      socket_name_(socket_name),
      protocol_handler_(NULL) {
}

DevToolsServer::~DevToolsServer() {
  Stop();
}

void DevToolsServer::Start() {
  if (protocol_handler_)
    return;

  chrome::VersionInfo version_info;

  protocol_handler_ = content::DevToolsHttpHandler::Start(
      new net::UnixDomainSocketWithAbstractNamespaceFactory(
          socket_name_,
          base::Bind(&content::CanUserConnectToDevTools)),
      use_bundled_frontend_resources_ ?
          "" :
          base::StringPrintf(kFrontEndURL, version_info.Version().c_str()),
      new DevToolsServerDelegate(use_bundled_frontend_resources_));
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

static jint InitRemoteDebugging(JNIEnv* env,
                                jobject obj,
                                jboolean use_bundled_frontend_resources,
                                jstring socketName) {
  DevToolsServer* server = new DevToolsServer(
      use_bundled_frontend_resources,
      base::android::ConvertJavaStringToUTF8(env, socketName));
  return reinterpret_cast<jint>(server);
}

static void DestroyRemoteDebugging(JNIEnv* env, jobject obj, jint server) {
  delete reinterpret_cast<DevToolsServer*>(server);
}

static jboolean IsRemoteDebuggingEnabled(JNIEnv* env,
                                         jobject obj,
                                         jint server) {
  return reinterpret_cast<DevToolsServer*>(server)->IsStarted();
}

static void SetRemoteDebuggingEnabled(JNIEnv* env,
                                      jobject obj,
                                      jint server,
                                      jboolean enabled) {
  DevToolsServer* devtools_server = reinterpret_cast<DevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start();
  } else {
    devtools_server->Stop();
  }
}
