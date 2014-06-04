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
#include "net/socket/unix_domain_socket_posix.h"
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
const char kDefaultSocketNamePrefix[] = "chrome";
const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

const char kTargetTypePage[] = "page";
const char kTargetTypeOther[] = "other";

static GURL GetFaviconURLForContents(WebContents* web_contents) {
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    return entry->GetFavicon().url;
  return GURL();
}

class TargetBase : public content::DevToolsTarget {
 public:
  // content::DevToolsTarget implementation:
  virtual std::string GetParentId() const OVERRIDE { return std::string(); }

  virtual std::string GetTitle() const OVERRIDE { return title_; }

  virtual std::string GetDescription() const OVERRIDE { return std::string(); }

  virtual GURL GetURL() const OVERRIDE { return url_; }

  virtual GURL GetFaviconURL() const OVERRIDE { return favicon_url_; }

  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }

 protected:
  explicit TargetBase(WebContents* web_contents)
      : title_(base::UTF16ToUTF8(web_contents->GetTitle())),
        url_(web_contents->GetURL()),
        favicon_url_(GetFaviconURLForContents(web_contents)),
        last_activity_time_(web_contents->GetLastActiveTime()) {
  }

  TargetBase(const base::string16& title, const GURL& url)
      : title_(base::UTF16ToUTF8(title)),
        url_(url)
  {}

 private:
  const std::string title_;
  const GURL url_;
  const GURL favicon_url_;
  const base::TimeTicks last_activity_time_;
};

class TabTarget : public TargetBase {
 public:
  static TabTarget* CreateForWebContents(int tab_id,
                                         WebContents* web_contents) {
    return new TabTarget(tab_id, web_contents);
  }

  static TabTarget* CreateForUnloadedTab(int tab_id,
                                         const base::string16& title,
                                         const GURL& url) {
    return new TabTarget(tab_id, title, url);
  }

  // content::DevToolsTarget implementation:
  virtual std::string GetId() const OVERRIDE {
    return base::IntToString(tab_id_);
  }

  virtual std::string GetType() const OVERRIDE { return kTargetTypePage; }

  virtual bool IsAttached() const OVERRIDE {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    WebContents* web_contents = model->GetWebContentsAt(index);
    if (!web_contents)
      return false;
    return DevToolsAgentHost::IsDebuggerAttached(web_contents);
  }

  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return NULL;
    WebContents* web_contents = model->GetWebContentsAt(index);
    if (!web_contents) {
      // The tab has been pushed out of memory, pull it back.
      TabAndroid* tab = model->GetTabAt(index);
      tab->LoadIfNeeded();
      web_contents = model->GetWebContentsAt(index);
      if (!web_contents)
        return NULL;
    }
    RenderViewHost* rvh = web_contents->GetRenderViewHost();
    return rvh ? DevToolsAgentHost::GetOrCreateFor(rvh) : NULL;
  }

  virtual bool Activate() const OVERRIDE {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->SetActiveIndex(index);
    return true;
  }

  virtual bool Close() const OVERRIDE {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->CloseTabAt(index);
    return true;
  }

 private:
  TabTarget(int tab_id, WebContents* web_contents)
      : TargetBase(web_contents),
        tab_id_(tab_id) {
  }

  TabTarget(int tab_id, const base::string16& title, const GURL& url)
      : TargetBase(title, url),
        tab_id_(tab_id) {
  }

  bool FindTab(TabModel** model_result, int* index_result) const {
    for (TabModelList::const_iterator iter = TabModelList::begin();
        iter != TabModelList::end(); ++iter) {
      TabModel* model = *iter;
      for (int i = 0; i < model->GetTabCount(); ++i) {
        TabAndroid* tab = model->GetTabAt(i);
        if (tab->GetAndroidId() == tab_id_) {
          *model_result = model;
          *index_result = i;
          return true;
        }
      }
    }
    return false;
  }

  const int tab_id_;
};

class NonTabTarget : public TargetBase {
 public:
  explicit NonTabTarget(WebContents* web_contents)
      : TargetBase(web_contents),
        agent_host_(DevToolsAgentHost::GetOrCreateFor(
            web_contents->GetRenderViewHost())) {
  }

  // content::DevToolsTarget implementation:
  virtual std::string GetId() const OVERRIDE {
    return agent_host_->GetId();
  }

  virtual std::string GetType() const OVERRIDE {
    if (TabModelList::begin() == TabModelList::end()) {
      // If there are no tab models we must be running in ChromeShell.
      // Return the 'page' target type for backwards compatibility.
      return kTargetTypePage;
    }
    return kTargetTypeOther;
  }

  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }

  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }

  virtual bool Activate() const OVERRIDE {
    RenderViewHost* rvh = agent_host_->GetRenderViewHost();
    if (!rvh)
      return false;
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      return false;
    web_contents->GetDelegate()->ActivateContents(web_contents);
    return true;
  }

  virtual bool Close() const OVERRIDE {
    RenderViewHost* rvh = agent_host_->GetRenderViewHost();
    if (!rvh)
      return false;
    rvh->ClosePage();
    return true;
  }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

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
        return std::string(data->front_as<char>(), data->size());
    }
    return "";
  }

  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) OVERRIDE {
    if (TabModelList::empty())
      return scoped_ptr<content::DevToolsTarget>();
    TabModel* tab_model = TabModelList::get(0);
    if (!tab_model)
      return scoped_ptr<content::DevToolsTarget>();
    WebContents* web_contents = tab_model->CreateNewTabForDevTools(url);
    if (!web_contents)
      return scoped_ptr<content::DevToolsTarget>();

    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (!tab)
      return scoped_ptr<content::DevToolsTarget>();

    return scoped_ptr<content::DevToolsTarget>(
        TabTarget::CreateForWebContents(tab->GetAndroidId(), web_contents));
  }

  virtual void EnumerateTargets(TargetCallback callback) OVERRIDE {
    TargetList targets;

    // Enumerate existing tabs, including the ones with no WebContents.
    std::set<WebContents*> tab_web_contents;
    for (TabModelList::const_iterator iter = TabModelList::begin();
        iter != TabModelList::end(); ++iter) {
      TabModel* model = *iter;
      for (int i = 0; i < model->GetTabCount(); ++i) {
        TabAndroid* tab = model->GetTabAt(i);
        WebContents* web_contents = model->GetWebContentsAt(i);
        if (web_contents) {
          tab_web_contents.insert(web_contents);
          targets.push_back(TabTarget::CreateForWebContents(tab->GetAndroidId(),
                                                            web_contents));
        } else {
          targets.push_back(TabTarget::CreateForUnloadedTab(tab->GetAndroidId(),
                                                            tab->GetTitle(),
                                                            tab->GetURL()));
        }
      }
    }

    // Add targets for WebContents not associated with any tabs.
    std::vector<RenderViewHost*> rvh_list =
        DevToolsAgentHost::GetValidRenderViewHosts();
    for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
         it != rvh_list.end(); ++it) {
      WebContents* web_contents = WebContents::FromRenderViewHost(*it);
      if (!web_contents)
        continue;
      if (tab_web_contents.find(web_contents) != tab_web_contents.end())
        continue;
      targets.push_back(new NonTabTarget(web_contents));
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
      base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str()),
      new DevToolsServerDelegate(),
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
                                      jboolean enabled) {
  DevToolsServer* devtools_server = reinterpret_cast<DevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start();
  } else {
    devtools_server->Stop();
  }
}
