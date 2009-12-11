// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_bridge.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_devtools_events.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"

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

bool ExtensionDevToolsBridge::RegisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  Browser* browser;
  TabStripModel* tab_strip;
  TabContents* contents;
  int tab_index;
  if (ExtensionTabUtil::GetTabById(tab_id_, profile_, &browser, &tab_strip,
                                   &contents, &tab_index)) {
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    devtools_manager->RegisterDevToolsClientHostFor(
        contents->render_view_host(), this);
    devtools_manager->ForwardToDevToolsAgent(
        this,
        DevToolsAgentMsg_SetApuAgentEnabled(true));
    return true;
  }
  return false;
}

void ExtensionDevToolsBridge::UnregisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  NotifyCloseListener();
}

// If the tab we are looking at is going away then we fire a closing event at
// the extension.
void ExtensionDevToolsBridge::InspectedTabClosing() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // TODO(knorton): Remove this event in favor of the standard tabs.onRemoved
  // event in extensions.
  std::string json("[{}]");
  profile_->GetExtensionMessageService()->
      DispatchEventToRenderers(on_tab_close_event_name_, json);

  // This may result in this object being destroyed.
  extension_devtools_manager_->BridgeClosingForTab(tab_id_);
}

void ExtensionDevToolsBridge::SendMessageToClient(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(ExtensionDevToolsBridge, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_RpcMessage, OnRpcMessage);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

static const char kApuAgentClassName[] = "ApuAgentDelegate";
static const char kApuPageEventMessageName[] = "DispatchToApu";

void ExtensionDevToolsBridge::OnRpcMessage(const DevToolsMessageData& data) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (data.class_name == kApuAgentClassName
      && data.method_name == kApuPageEventMessageName) {
    std::string json = StringPrintf("[%s]", data.arguments[0].c_str());
    profile_->GetExtensionMessageService()->DispatchEventToRenderers(
        on_page_event_name_, json);
  }
}

