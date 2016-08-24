// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_discovery_provider_android.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::WebContents;
using devtools_discovery::DevToolsTargetDescriptor;

namespace {

class TabProxyDelegate : public content::DevToolsExternalAgentProxyDelegate,
                         public content::DevToolsAgentHostClient {
 public:
  TabProxyDelegate(int tab_id, const base::string16& title, const GURL& url)
      : tab_id_(tab_id),
        title_(base::UTF16ToUTF8(title)),
        url_(url) {
  }

  ~TabProxyDelegate() override {
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    proxy_->DispatchOnClientHost(message);
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override {
    proxy_->ConnectionClosed();
  }

  void Attach(content::DevToolsExternalAgentProxy* proxy) override {
    proxy_ = proxy;
    MaterializeAgentHost();
  }

  void Detach() override {
    if (agent_host_)
      agent_host_->DetachClient(this);
    agent_host_ = nullptr;
    proxy_ = nullptr;
  }

  std::string GetType() override {
    return DevToolsAgentHost::kTypePage;
  }

  std::string GetTitle() override {
    return title_;
  }

  std::string GetDescription() override {
    return std::string();
  }

  GURL GetURL() override {
    return url_;
  }

  GURL GetFaviconURL() override {
    return GURL();
  }

  bool Activate() override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->SetActiveIndex(index);
    return true;
  }

  bool Inspect() override {
    MaterializeAgentHost();
    if (agent_host_)
      return agent_host_->Inspect();
    return false;
  }

  void Reload() override {
    MaterializeAgentHost();
    if (agent_host_)
      agent_host_->Reload();
  }

  bool Close() override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->CloseTabAt(index);
    return true;
  }

  void SendMessageToBackend(const std::string& message) override {
    if (agent_host_)
      agent_host_->DispatchProtocolMessage(this, message);
  }

 private:
  void MaterializeAgentHost() {
    if (agent_host_)
      return;
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return;
    WebContents* web_contents = model->GetWebContentsAt(index);
    if (!web_contents)
      return;
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
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
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  content::DevToolsExternalAgentProxy* proxy_;
  DISALLOW_COPY_AND_ASSIGN(TabProxyDelegate);
};

std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor>
CreateNewAndroidTab(const GURL& url) {
  if (TabModelList::empty())
    return std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  TabModel* tab_model = TabModelList::get(0);
  if (!tab_model)
    return std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  WebContents* web_contents = tab_model->CreateNewTabForDevTools(url);
  if (!web_contents)
    return std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor>();

  scoped_refptr<content::DevToolsAgentHost> host =
      content::DevToolsAgentHost::Create(new TabProxyDelegate(
           tab->GetAndroidId(), tab->GetTitle(), tab->GetURL()));
  return base::WrapUnique(new devtools_discovery::BasicTargetDescriptor(host));
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
        result.push_back(new devtools_discovery::BasicTargetDescriptor(
            content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
      } else {
        scoped_refptr<content::DevToolsAgentHost> host =
            content::DevToolsAgentHost::Create(new TabProxyDelegate(
                tab->GetAndroidId(), tab->GetTitle(), tab->GetURL()));
        result.push_back(new devtools_discovery::BasicTargetDescriptor(host));
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
      base::WrapUnique(new DevToolsDiscoveryProviderAndroid()));
  discovery_manager->SetCreateCallback(base::Bind(&CreateNewAndroidTab));
}
