// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#import <Cocoa/Cocoa.h>

#include "app/app_switches.h"
#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/debug_util.h"
#include "chrome/app/breakpad_mac.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_main_win.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#import "chrome/browser/cocoa/keystone_glue.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/result_codes.h"

namespace Platform {

// Tell Cooca to finish its initalization, which we want to do manually
// instead of calling NSApplicationMain(). The primary reason is that NSAM()
// never returns, which would leave all the objects currently on the stack
// in scoped_ptrs hanging and never cleaned up. We then load the main nib
// directly. The main event loop is run from common code using the
// MessageLoop API, which works out ok for us because it's a wrapper around
// CFRunLoop.
void WillInitializeMainMessageLoop(const MainFunctionParams& parameters) {
  // Initialize NSApplication using the custom subclass.
  [BrowserCrApplication sharedApplication];

  // Before we load the nib, we need to start up the resource bundle so we have
  // the strings avaiable for localization.
  if (!parameters.ui_task) {
    ResourceBundle::InitSharedInstance(std::wstring());
  }
  // Now load the nib.
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

  // The browser process only wants to support the language Cocoa will use, so
  // force the app locale to be overriden with that value.
  l10n_util::OverrideLocaleWithCocoaLocale();

  // This is a no-op if the KeystoneRegistration framework is not present.
  // The framework is only distributed with branded Google Chrome builds.
  [[KeystoneGlue defaultKeystoneGlue] registerWithKeystone];
}

void DidEndMainMessageLoop() {
  AppController* appController = [NSApp delegate];
  [appController didEndMainMessageLoop];
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  metrics->RecordBreakpadRegistration(IsCrashReporterEnabled());
  metrics->RecordBreakpadHasDebugger(DebugUtil::BeingDebugged());
}

}  // namespace Platform

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return ResultCodes::NORMAL_EXIT;
}

bool DoUpgradeTasks(const CommandLine& command_line) {
  return false;
}

bool CheckForWin2000() {
  return false;
}

int HandleIconsCommands(const CommandLine& parsed_command_line) {
  return 0;
}

bool CheckMachineLevelInstall() {
  return false;
}

void PrepareRestartOnCrashEnviroment(const CommandLine& parsed_command_line) {
}
