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
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "grit/devtools_discovery_page_resources.h"
#include "jni/DevToolsServer_jni.h"
#include "net/base/escape.h"
#include "net/socket/unix_domain_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/common/user_agent/user_agent_util.h"

using content::DevToolsAgentHost;
using content::WebContents;

namespace {

const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/devtools.html";
const char kDefaultSocketNamePrefix[] = "chrome";
const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

const char kTargetTypePage[] = "page";

bool FindTab(const std::string& str_id,
             TabModel** model_result,
             int* index_result) {
  int id;
  if (!base::StringToInt(str_id, &id))
    return false;

  for (TabModelList::const_iterator iter = TabModelList::begin();
      iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (id == tab->GetAndroidId()) {
        *model_result = model;
        *index_result = i;
        return true;
      }
    }
  }
  return false;
}


class Target : public content::DevToolsTarget {
 public:
  // Constructor for a tab with a valid WebContents.
  Target(int id, WebContents* web_contents);

  // Constructor for a tab unloaded from memory.
  Target(int id, const string16& title, const GURL& url);

  virtual std::string GetId() const OVERRIDE { return id_; }
  virtual std::string GetType() const OVERRIDE { return kTargetTypePage; }
  virtual std::string GetTitle() const OVERRIDE { return title_; }
  virtual std::string GetDescription() const OVERRIDE { return std::string(); }
  virtual GURL GetUrl() const OVERRIDE { return url_; }
  virtual GURL GetFaviconUrl() const OVERRIDE { return favicon_url_; }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE;
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE;
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

 private:
  std::string id_;
  std::string title_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

Target::Target(int id, WebContents* web_contents) {
  id_ = base::IntToString(id);
  title_ = UTF16ToUTF8(net::EscapeForHTML(web_contents->GetTitle()));
  url_ = web_contents->GetURL();
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    favicon_url_ = entry->GetFavicon().url;
  last_activity_time_ = web_contents->GetLastSelectedTime();
}

Target::Target(int id, const string16& title, const GURL& url) {
  id_ = base::IntToString(id);
  title_ = UTF16ToUTF8(net::EscapeForHTML(title));
  url_ = url;
}

bool Target::IsAttached() const {
  TabModel* model;
  int index;
  if (!FindTab(id_, &model, &index))
    return false;
  WebContents* web_contents = model->GetWebContentsAt(index);
  if (!web_contents)
    return false;
  return DevToolsAgentHost::IsDebuggerAttached(web_contents);
}

scoped_refptr<DevToolsAgentHost> Target::GetAgentHost() const {
  TabModel* model;
  int index;
  if (!FindTab(id_, &model, &index))
    return NULL;
  WebContents* web_contents = model->GetWebContentsAt(index);
  if (!web_contents) {
    // The tab has been pushed out of memory, pull it back.
    TabAndroid* tab = model->GetTabAt(index);
    tab->RestoreIfNeeded();
    web_contents = model->GetWebContentsAt(index);
    if (!web_contents)
      return NULL;
  }
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  return rvh ? DevToolsAgentHost::GetOrCreateFor(rvh) : NULL;
}

bool Target::Activate() const {
  TabModel* model;
  int index;
  if (!FindTab(id_, &model, &index))
    return false;
  model->SetActiveIndex(index);
  return true;
}

bool Target::Close() const {
  TabModel* model;
  int index;
  if (!FindTab(id_, &model, &index))
    return false;
  model->CloseTabAt(index);
  return true;
}

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
      if (top_sites->GetPageThumbnail(url, false, &data))
        return std::string(reinterpret_cast<const char*>(data->front()),
                           data->size());
    }
    return "";
  }

  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget() OVERRIDE {
    Profile* profile =
        g_browser_process->profile_manager()->GetDefaultProfile();
    TabModel* tab_model = TabModelList::GetTabModelWithProfile(profile);
    if (!tab_model)
      return scoped_ptr<content::DevToolsTarget>();
    WebContents* web_contents =
        tab_model->CreateTabForTesting(GURL(content::kAboutBlankURL));
    if (!web_contents)
      return scoped_ptr<content::DevToolsTarget>();

    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      if (web_contents != tab_model->GetWebContentsAt(i))
        continue;
      TabAndroid* tab = tab_model->GetTabAt(i);
      return scoped_ptr<content::DevToolsTarget>(
          new Target(tab->GetAndroidId(), web_contents));
    }

    return scoped_ptr<content::DevToolsTarget>();
  }

  virtual void EnumerateTargets(TargetCallback callback) OVERRIDE {
    TargetList targets;
    for (TabModelList::const_iterator iter = TabModelList::begin();
        iter != TabModelList::end(); ++iter) {
      TabModel* model = *iter;
      for (int i = 0; i < model->GetTabCount(); ++i) {
        TabAndroid* tab = model->GetTabAt(i);
        WebContents* web_contents = model->GetWebContentsAt(i);
        targets.push_back(
            web_contents ?
                new Target(tab->GetAndroidId(), web_contents) :
                new Target(
                    tab->GetAndroidId(), tab->GetTitle(), tab->GetURL()));
      }
    }
    callback.Run(targets);
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
