// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_aurawin.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metro_viewer/chrome_metro_viewer_process_host_aurawin.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/base/ui_base_switches.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() {
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    // Tell metro viewer to close when we are shutting down.
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::OnMetroViewerProcessTerminated() {
  metro_viewer_process_host_.reset(NULL);
}

void BrowserProcessPlatformPart::PlatformSpecificCommandLineProcessing(
    const CommandLine& command_line) {
  // Check for Windows 8 specific commandlines requesting that this process
  // either connect to an existing viewer or launch a new viewer and
  // synchronously wait for it to connect.

  bool launch = command_line.HasSwitch(switches::kViewerLaunchViaAppId);
  bool connect = (launch ||
                  (command_line.HasSwitch(switches::kViewerConnect) &&
                   !metro_viewer_process_host_.get()));
  if (!connect)
    return;
  // Create a host to connect to the Metro viewer process over IPC.
  metro_viewer_process_host_.reset(new ChromeMetroViewerProcessHost());
  if (launch) {
    CHECK(metro_viewer_process_host_->LaunchViewerAndWaitForConnection(
        command_line.GetSwitchValueNative(
            switches::kViewerLaunchViaAppId)));
  }

}

void BrowserProcessPlatformPart::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {

  DCHECK(type == chrome::NOTIFICATION_APP_TERMINATING);
  PrefService* pref_service = g_browser_process->local_state();
  bool is_relaunch = pref_service->GetBoolean(prefs::kWasRestarted);
  if (is_relaunch) {
    upgrade_util::RelaunchMode mode =
        upgrade_util::RelaunchModeStringToEnum(
            pref_service->GetString(prefs::kRelaunchMode));
    if (metro_viewer_process_host_.get()) {
      if (mode == upgrade_util::RELAUNCH_MODE_DESKTOP) {
        // Metro -> Desktop
        chrome::ActivateDesktopHelper(chrome::ASH_TERMINATE);
      } else {
        // Metro -> Metro
        ChromeMetroViewerProcessHost::HandleMetroExit();
      }
    }
  }
}
