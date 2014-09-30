// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_manager_delegate_android.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::WebContents;

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeOther[] = "other";

GURL GetFaviconURLForContents(WebContents* web_contents) {
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    return entry->GetFavicon().url;
  return GURL();
}

GURL GetFaviconURLForAgentHost(
    scoped_refptr<DevToolsAgentHost> agent_host) {
  if (WebContents* web_contents = agent_host->GetWebContents())
    return GetFaviconURLForContents(web_contents);
  return GURL();
}

base::TimeTicks GetLastActiveTimeForAgentHost(
    scoped_refptr<DevToolsAgentHost> agent_host) {
  if (WebContents* web_contents = agent_host->GetWebContents())
    return web_contents->GetLastActiveTime();
  return base::TimeTicks();
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

  explicit TargetBase(scoped_refptr<DevToolsAgentHost> agent_host)
      : title_(agent_host->GetTitle()),
        url_(agent_host->GetURL()),
        favicon_url_(GetFaviconURLForAgentHost(agent_host)),
        last_activity_time_(GetLastActiveTimeForAgentHost(agent_host)) {
  }

  TargetBase(const std::string& title, const GURL& url)
      : title_(title),
        url_(url) {
  }

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

  virtual std::string GetType() const OVERRIDE {
    return kTargetTypePage;
  }

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
      if (!tab)
        return NULL;

      if (!tab->LoadIfNeeded())
        return NULL;

      web_contents = model->GetWebContentsAt(index);
      if (!web_contents)
        return NULL;
    }
    return DevToolsAgentHost::GetOrCreateFor(web_contents);
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
      : TargetBase(base::UTF16ToUTF8(title), url),
        tab_id_(tab_id) {
  }

  bool FindTab(TabModel** model_result, int* index_result) const {
    for (TabModelList::const_iterator iter = TabModelList::begin();
        iter != TabModelList::end(); ++iter) {
      TabModel* model = *iter;
      for (int i = 0; i < model->GetTabCount(); ++i) {
        TabAndroid* tab = model->GetTabAt(i);
        if (tab && tab->GetAndroidId() == tab_id_) {
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
  explicit NonTabTarget(scoped_refptr<DevToolsAgentHost> agent_host)
      : TargetBase(agent_host),
        agent_host_(agent_host) {
  }

  // content::DevToolsTarget implementation:
  virtual std::string GetId() const OVERRIDE {
    return agent_host_->GetId();
  }

  virtual std::string GetType() const OVERRIDE {
    switch (agent_host_->GetType()) {
      case DevToolsAgentHost::TYPE_WEB_CONTENTS:
        if (TabModelList::begin() == TabModelList::end()) {
          // If there are no tab models we must be running in ChromeShell.
          // Return the 'page' target type for backwards compatibility.
          return kTargetTypePage;
        }
        break;
      case DevToolsAgentHost::TYPE_SERVICE_WORKER:
        return kTargetTypeServiceWorker;
      default:
        break;
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
    return agent_host_->Activate();
  }

  virtual bool Close() const OVERRIDE {
    return agent_host_->Close();
  }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

}  // namespace

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
}

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() {
}

void DevToolsManagerDelegateAndroid::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
}

base::DictionaryValue* DevToolsManagerDelegateAndroid::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

void DevToolsManagerDelegateAndroid::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}

scoped_ptr<content::DevToolsTarget>
    DevToolsManagerDelegateAndroid::CreateNewTarget(const GURL& url) {
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

void DevToolsManagerDelegateAndroid::EnumerateTargets(TargetCallback callback) {
  TargetList targets;

  // Enumerate existing tabs, including the ones with no WebContents.
  std::set<WebContents*> tab_web_contents;
  for (TabModelList::const_iterator iter = TabModelList::begin();
      iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (!tab)
        continue;

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
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    if (WebContents* web_contents = (*it)->GetWebContents()) {
      if (tab_web_contents.find(web_contents) != tab_web_contents.end())
        continue;
    }
    targets.push_back(new NonTabTarget(*it));
  }

  callback.Run(targets);
}

std::string DevToolsManagerDelegateAndroid::GetPageThumbnailData(
    const GURL& url) {
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  history::TopSites* top_sites = profile->GetTopSites();
  if (top_sites) {
    scoped_refptr<base::RefCountedMemory> data;
    if (top_sites->GetPageThumbnail(url, false, &data))
      return std::string(data->front_as<char>(), data->size());
  }
  return std::string();
}
