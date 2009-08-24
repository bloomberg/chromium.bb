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
      inspected_rvh_(NULL),
      profile_(profile),
      on_page_event_name_(
          ExtensionDevToolsEvents::OnPageEventNameForTab(tab_id)),
      on_tab_url_change_event_name_(
          ExtensionDevToolsEvents::OnTabUrlChangeEventNameForTab(tab_id)),
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
    inspected_rvh_ = contents->render_view_host();
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        inspected_rvh_, this);
    return true;
  }
  return false;
}

void ExtensionDevToolsBridge::UnregisterAsDevToolsClientHost() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (inspected_rvh_) {
    DevToolsManager::GetInstance()->UnregisterDevToolsClientHostFor(
        inspected_rvh_);
    inspected_rvh_ = NULL;
  }
}

// If the tab we are looking at is going away then we fire a closing event at
// the extension.
void ExtensionDevToolsBridge::InspectedTabClosing() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

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

static const char kTimelineAgentClassName[] = "TimelineAgentClass";
static const char kPageEventMessageName[] = "PageEventMessage";
static const char kTabUrlChangeEventMessageName[] = "TabUrlChangeEventMessage";

void ExtensionDevToolsBridge::OnRpcMessage(const std::string& class_name,
                                           const std::string& message_name,
                                           const std::string& msg) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  // TODO(jamesr): Update the filtering and message creation logic once
  // the TimelineAgent lands in WebKit.
  if (class_name == kTimelineAgentClassName) {
    if (message_name == kPageEventMessageName) {
      std::string json = StringPrintf("[{\"record\": \"%s\"}]", msg.c_str());
      profile_->GetExtensionMessageService()->
          DispatchEventToRenderers(on_page_event_name_, json);
    } else if (message_name == kTabUrlChangeEventMessageName) {
      std::string json = StringPrintf("[{\"record\": \"%s\"}]", msg.c_str());
      profile_->GetExtensionMessageService()->
          DispatchEventToRenderers(on_tab_url_change_event_name_, json);
    }
  }
}

