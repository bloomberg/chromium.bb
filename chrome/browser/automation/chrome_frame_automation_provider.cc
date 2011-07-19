// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/chrome_frame_automation_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/automation_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_channel.h"

ChromeFrameAutomationProvider::ChromeFrameAutomationProvider(Profile* profile)
    : AutomationProvider(profile) {}

bool ChromeFrameAutomationProvider::OnMessageReceived(
    const IPC::Message& message) {
  if (IsValidMessage(message.type()))
    return AutomationProvider::OnMessageReceived(message);

  OnUnhandledMessage(message);
  return false;
}

void ChromeFrameAutomationProvider::OnUnhandledMessage(
    const IPC::Message& message) {
  NOTREACHED() << __FUNCTION__
               << " Unhandled message type: "
               << message.type();
}

bool ChromeFrameAutomationProvider::IsValidMessage(uint32 type) {
  bool is_valid_message = false;

  switch (type) {
    case AutomationMsg_CreateExternalTab::ID:
    case AutomationMsg_ConnectExternalTab::ID:
#if defined(OS_WIN)
    case AutomationMsg_BrowserMove::ID:
    case AutomationMsg_ProcessUnhandledAccelerator::ID:
    case AutomationMsg_TabReposition::ID:
    case AutomationMsg_ForwardContextMenuCommandToChrome::ID:
#endif  // defined(OS_WIN)
    case AutomationMsg_NavigateInExternalTab::ID:
    case AutomationMsg_NavigateExternalTabAtIndex::ID:
    case AutomationMsg_Find::ID:
    case AutomationMsg_SetInitialFocus::ID:
    case AutomationMsg_SetPageFontSize::ID:
    case AutomationMsg_SetProxyConfig::ID:
    case AutomationMsg_Cut::ID:
    case AutomationMsg_Copy::ID:
    case AutomationMsg_Paste::ID:
    case AutomationMsg_SelectAll::ID:
    case AutomationMsg_ReloadAsync::ID:
    case AutomationMsg_StopAsync::ID:
    case AutomationMsg_PrintAsync::ID:
    case AutomationMsg_HandleUnused::ID:
    case AutomationMsg_HandleMessageFromExternalHost::ID:
    case AutomationMsg_RequestStarted::ID:
    case AutomationMsg_RequestData::ID:
    case AutomationMsg_RequestEnd::ID:
    case AutomationMsg_SaveAsAsync::ID:
    case AutomationMsg_RemoveBrowsingData::ID:
    case AutomationMsg_OverrideEncoding::ID:
    case AutomationMsg_RunUnloadHandlers::ID:
    case AutomationMsg_SetZoomLevel::ID: {
      is_valid_message = true;
      break;
    }

    default:
      break;
  }

  return is_valid_message;
}
