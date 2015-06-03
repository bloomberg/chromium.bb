// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_discovery_provider_android.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::WebContents;

namespace {

GURL GetFaviconURLForContents(WebContents* web_contents) {
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    return entry->GetFavicon().url;
  return GURL();
}

class TabDescriptor : public devtools_discovery::DevToolsTargetDescriptor {
 public:
  static TabDescriptor* CreateForWebContents(int tab_id,
                                             WebContents* web_contents) {
    return new TabDescriptor(tab_id, web_contents);
  }

  static TabDescriptor* CreateForUnloadedTab(int tab_id,
                                             const base::string16& title,
                                             const GURL& url) {
    return new TabDescriptor(tab_id, title, url);
  }

  ~TabDescriptor() override {
  }

  // devtools_discovery::DevToolsTargetDescriptor implementation.
  std::string GetParentId() const override {
    return std::string();
  }

  std::string GetTitle() const override {
    return title_;
  }

  std::string GetDescription() const override {
    return std::string();
  }

  GURL GetURL() const override {
    return url_;
  }

  GURL GetFaviconURL() const override {
    return favicon_url_;
  }

  base::TimeTicks GetLastActivityTime() const override {
    return last_activity_time_;
  }

  std::string GetId() const override {
    return base::IntToString(tab_id_);
  }

  std::string GetType() const override {
    return devtools_discovery::BasicTargetDescriptor::kTypePage;
  }

  bool IsAttached() const override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    WebContents* web_contents = model->GetWebContentsAt(index);
    if (!web_contents)
      return false;
    return DevToolsAgentHost::IsDebuggerAttached(web_contents);
  }

  scoped_refptr<DevToolsAgentHost> GetAgentHost() const override {
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

  bool Activate() const override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->SetActiveIndex(index);
    return true;
  }

  bool Close() const override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->CloseTabAt(index);
    return true;
  }

 private:
  TabDescriptor(int tab_id, WebContents* web_contents)
      : tab_id_(tab_id),
        title_(base::UTF16ToUTF8(web_contents->GetTitle())),
        url_(web_contents->GetURL()),
        favicon_url_(GetFaviconURLForContents(web_contents)),
        last_activity_time_(web_contents->GetLastActiveTime()) {
  }

  TabDescriptor(int tab_id, const base::string16& title, const GURL& url)
      : tab_id_(tab_id),
        title_(base::UTF16ToUTF8(title)),
        url_(url) {
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
  const std::string title_;
  const GURL url_;
  const GURL favicon_url_;
  const base::TimeTicks last_activity_time_;

  DISALLOW_COPY_AND_ASSIGN(TabDescriptor);
};

scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>
CreateNewAndroidTab(const GURL& url) {
  if (TabModelList::empty())
    return scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  TabModel* tab_model = TabModelList::get(0);
  if (!tab_model)
    return scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  WebContents* web_contents = tab_model->CreateNewTabForDevTools(url);
  if (!web_contents)
    return scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  return make_scoped_ptr(TabDescriptor::CreateForWebContents(
      tab->GetAndroidId(), web_contents));
}

}  // namespace

DevToolsDiscoveryProviderAndroid::DevToolsDiscoveryProviderAndroid() {
}

DevToolsDiscoveryProviderAndroid::~DevToolsDiscoveryProviderAndroid() {
}

devtools_discovery::DevToolsTargetDescriptor::List
DevToolsDiscoveryProviderAndroid::GetDescriptors() {
  devtools_discovery::DevToolsTargetDescriptor::List result;

  // Enumerate existing tabs, including the ones with no WebContents.
  std::set<WebContents*> tab_web_contents;
  for (TabModelList::const_iterator iter = TabModelList::begin();
      iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (!tab)
        continue;

      WebContents* web_contents = tab->web_contents();
      if (web_contents) {
        tab_web_contents.insert(web_contents);
        result.push_back(TabDescriptor::CreateForWebContents(
            tab->GetAndroidId(), web_contents));
      } else {
        result.push_back(TabDescriptor::CreateForUnloadedTab(
            tab->GetAndroidId(), tab->GetTitle(), tab->GetURL()));
      }
    }
  }

  // Add descriptors for targets not associated with any tabs.
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    if (WebContents* web_contents = (*it)->GetWebContents()) {
      if (tab_web_contents.find(web_contents) != tab_web_contents.end())
        continue;
    }
    result.push_back(new devtools_discovery::BasicTargetDescriptor(*it));
  }

  return result;
}

// static
void DevToolsDiscoveryProviderAndroid::Install() {
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  discovery_manager->AddProvider(
      make_scoped_ptr(new DevToolsDiscoveryProviderAndroid()));
  discovery_manager->SetCreateCallback(base::Bind(&CreateNewAndroidTab));
}
