// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/chrome_frame_automation_provider_win.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_switches.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

const int kMaxChromeShutdownDelaySeconds = 60*60;

ChromeFrameAutomationProvider::ChromeFrameAutomationProvider(Profile* profile)
    : AutomationProvider(profile) {
  DCHECK(g_browser_process);
  if (g_browser_process)
    g_browser_process->AddRefModule();
}

ChromeFrameAutomationProvider::~ChromeFrameAutomationProvider() {
  DCHECK(g_browser_process);
  if (g_browser_process) {
    CommandLine& cmd_line = *CommandLine::ForCurrentProcess();

    CommandLine::StringType shutdown_delay(
        cmd_line.GetSwitchValueNative(switches::kChromeFrameShutdownDelay));
    if (!shutdown_delay.empty()) {
      VLOG(1) << "ChromeFrameAutomationProvider: "
                 "Scheduling ReleaseBrowserProcess.";

      // Grab the specified shutdown delay.
      int shutdown_delay_seconds = 0;
      base::StringToInt(shutdown_delay, &shutdown_delay_seconds);

      // Clamp to reasonable values.
      shutdown_delay_seconds = std::max(0, shutdown_delay_seconds);
      shutdown_delay_seconds = std::min(shutdown_delay_seconds,
                                        kMaxChromeShutdownDelaySeconds);

      // We have Chrome Frame defer Chrome shutdown for a time to improve
      // intra-page load times.
      // Note that we are tracking the perf impact of this under
      // http://crbug.com/98506
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ChromeFrameAutomationProvider::ReleaseBrowserProcess),
          base::TimeDelta::FromSeconds(shutdown_delay_seconds));
    } else {
      VLOG(1) << "ChromeFrameAutomationProvider: "
                 "Releasing browser module with no delay.";
      g_browser_process->ReleaseModule();
    }
  }
}

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
    case AutomationMsg_BrowserMove::ID:
    case AutomationMsg_ProcessUnhandledAccelerator::ID:
    case AutomationMsg_ForwardContextMenuCommandToChrome::ID:
    case AutomationMsg_TabReposition::ID:
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

// static
void ChromeFrameAutomationProvider::ReleaseBrowserProcess() {
  if (g_browser_process) {
    VLOG(1) << "ChromeFrameAutomationProvider: Releasing browser process.";
    g_browser_process->ReleaseModule();
  }
}
