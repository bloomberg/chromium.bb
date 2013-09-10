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
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "grit/devtools_discovery_page_resources.h"
#include "jni/DevToolsServer_jni.h"
#include "net/socket/unix_domain_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/common/user_agent/user_agent_util.h"

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/devtools.html";
const char kDefaultSocketNamePrefix[] = "chrome";
const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

// Delegate implementation for the devtools http handler on android. A new
// instance of this gets created each time devtools is enabled.
class DevToolsServerDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  DevToolsServerDelegate()
      : last_tethering_socket_(0) {
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
    Profile* profile =
        g_browser_process->profile_manager()->GetDefaultProfile();
    TabModel* tab_model = TabModelList::GetTabModelWithProfile(profile);
    if (!tab_model)
      return NULL;
    content::WebContents* web_contents =
        tab_model->CreateTabForTesting(GURL(content::kAboutBlankURL));
    if (!web_contents)
      return NULL;
    return web_contents->GetRenderViewHost();
  }

  virtual TargetType GetTargetType(content::RenderViewHost*) OVERRIDE {
    return kTargetTypeTab;
  }

  virtual std::string GetViewDescription(content::RenderViewHost*) OVERRIDE {
    return "";
  }

  virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(
      net::StreamListenSocket::Delegate* delegate,
      std::string* name) OVERRIDE {
    *name = base::StringPrintf(
        kTetheringSocketName, getpid(), ++last_tethering_socket_);
    return net::UnixDomainSocket::CreateAndListenWithAbstractNamespace(
               *name,
               "",
               delegate,
               base::Bind(&content::CanUserConnectToDevTools))
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

  DISALLOW_COPY_AND_ASSIGN(DevToolsServerDelegate);
};

}  // namespace

DevToolsServer::DevToolsServer()
    : socket_name_(base::StringPrintf(kDevToolsChannelNameFormat,
                                      kDefaultSocketNamePrefix)),
      protocol_handler_(NULL) {
  // Override the default socket name if one is specified on the command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name_ = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
}

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

void DevToolsServer::Start() {
  if (protocol_handler_)
    return;

  protocol_handler_ = content::DevToolsHttpHandler::Start(
      new net::UnixDomainSocketWithAbstractNamespaceFactory(
          socket_name_,
          base::StringPrintf("%s_%d", socket_name_.c_str(), getpid()),
          base::Bind(&content::CanUserConnectToDevTools)),
      base::StringPrintf(kFrontEndURL,
                         webkit_glue::GetWebKitRevision().c_str()),
      new DevToolsServerDelegate());
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
                                jstring socket_name_prefix) {
  DevToolsServer* server = new DevToolsServer(
      base::android::ConvertJavaStringToUTF8(env, socket_name_prefix));
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
