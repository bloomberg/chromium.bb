// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_bridge.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_devtools_events.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsManager;
using content::WebContents;

ExtensionDevToolsBridge::ExtensionDevToolsBridge(int tab_id,
                                                 Profile* profile)
    : tab_id_(tab_id),
      profile_(profile),
      on_page_event_name_(
          ExtensionDevToolsEvents::OnPageEventNameForTab(tab_id)),
      on_tab_close_event_name_(
          ExtensionDevToolsEvents::OnTabCloseEventNameForTab(tab_id)) {
  extension_devtools_manager_ =
      extensions::ExtensionSystem::Get(profile)->devtools_manager();
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
  base::JSONWriter::Write(&message, &json);
  return json;
}

bool ExtensionDevToolsBridge::RegisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  Browser* browser;
  TabStripModel* tab_strip;
  WebContents* contents;
  int tab_index;
  if (ExtensionTabUtil::GetTabById(tab_id_, profile_, true,
                                   &browser, &tab_strip,
                                   &contents, &tab_index)) {
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
        contents->GetRenderViewHost());
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

    // 3. Enable page events.
    devtools_manager->DispatchOnInspectorBackend(
        this,
        FormatDevToolsMessage(4, "Page.enable"));

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
void ExtensionDevToolsBridge::InspectedContentsClosing() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // TODO(knorton): Remove this event in favor of the standard tabs.onRemoved
  // event in extensions.
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  arguments->Set(0, new base::DictionaryValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      on_tab_close_event_name_, arguments.Pass()));
  event->restrict_to_profile = profile_;
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());

  // This may result in this object being destroyed.
  extension_devtools_manager_->BridgeClosingForTab(tab_id_);
}

void ExtensionDevToolsBridge::DispatchOnInspectorFrontend(
    const std::string& data) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  if (!data.empty()) {
    arguments->Append(base::JSONReader::Read(data));
  }

  scoped_ptr<extensions::Event> event(new extensions::Event(
      on_page_event_name_, arguments.Pass()));
  event->restrict_to_profile = profile_;
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void ExtensionDevToolsBridge::ContentsReplaced(WebContents* new_contents) {
  // We don't update the tab id as it needs to remain the same so that we can
  // properly unregister.
}

void ExtensionDevToolsBridge::ReplacedWithAnotherClient() {
}
