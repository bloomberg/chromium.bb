// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_bridge.h"

#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_devtools_events.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_manager.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsManager;

ExtensionDevToolsBridge::ExtensionDevToolsBridge(int tab_id,
                                                 Profile* profile)
    : tab_id_(tab_id),
      profile_(profile),
      on_page_event_name_(
          ExtensionDevToolsEvents::OnPageEventNameForTab(tab_id)),
      on_tab_close_event_name_(
          ExtensionDevToolsEvents::OnTabCloseEventNameForTab(tab_id)) {
  extension_devtools_manager_ = profile_->GetExtensionDevToolsManager();
  DCHECK(extension_devtools_manager_.get());
}

ExtensionDevToolsBridge::~ExtensionDevToolsBridge() {
}

static std::string FormatDevToolsMessage(int id, const std::string& method) {
  DictionaryValue message;
  message.SetInteger("id", id);
  message.SetString("method", method);
  message.Set("params", new DictionaryValue);

  std::string json;
  base::JSONWriter::Write(&message, false, &json);
  return json;
}

bool ExtensionDevToolsBridge::RegisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  Browser* browser;
  TabStripModel* tab_strip;
  TabContentsWrapper* contents;
  int tab_index;
  if (ExtensionTabUtil::GetTabById(tab_id_, profile_, true,
                                   &browser, &tab_strip,
                                   &contents, &tab_index)) {
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
        contents->render_view_host());
    if (devtools_manager->GetDevToolsClientHostFor(agent))
      return false;

    devtools_manager->RegisterDevToolsClientHostFor(agent, this);

    // Following messages depend on inspector protocol that is not yet
    // finalized.

    // 1. Start timeline profiler.
    devtools_manager->DispatchOnInspectorBackend(
        this,
        FormatDevToolsMessage(2, "Timeline.start"));

    // 2. Enable network resource tracking.
    devtools_manager->DispatchOnInspectorBackend(
        this,
        FormatDevToolsMessage(3, "Network.enable"));

    return true;
  }
  return false;
}

void ExtensionDevToolsBridge::UnregisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DevToolsManager::GetInstance()->ClientHostClosing(this);
}

// If the tab we are looking at is going away then we fire a closing event at
// the extension.
void ExtensionDevToolsBridge::InspectedTabClosing() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // TODO(knorton): Remove this event in favor of the standard tabs.onRemoved
  // event in extensions.
  std::string json("[{}]");
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      on_tab_close_event_name_, json, profile_, GURL());

  // This may result in this object being destroyed.
  extension_devtools_manager_->BridgeClosingForTab(tab_id_);
}

void ExtensionDevToolsBridge::DispatchOnInspectorFrontend(
    const std::string& data) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  std::string json = base::StringPrintf("[%s]", data.c_str());
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      on_page_event_name_, json, profile_, GURL());
}

void ExtensionDevToolsBridge::TabReplaced(TabContents* new_tab) {
  // We don't update the tab id as it needs to remain the same so that we can
  // properly unregister.
}
