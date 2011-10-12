// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_aura.h"

#include "base/logging.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/main_function_params.h"
#endif

ChromeBrowserMainPartsAura::ChromeBrowserMainPartsAura(
    const MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
  NOTIMPLEMENTED();
}

ChromeBrowserMainPartsAura::~ChromeBrowserMainPartsAura() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Shutdown();
  if (!parameters().ui_task && chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Shutdown();
#endif
}

void ChromeBrowserMainPartsAura::PreEarlyInitialization() {
  NOTIMPLEMENTED();
}

void ChromeBrowserMainPartsAura::PreMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  if (!parameters().ui_task) {
    bool use_stub = parameters().command_line_.HasSwitch(switches::kStubCros);
    chromeos::CrosLibrary::Initialize(use_stub);
  }
#endif
}

void ChromeBrowserMainPartsAura::PostMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  // Initialize DBusThreadManager for the browser. This must be done after
  // the main message loop is started, as it uses the message loop.
  chromeos::DBusThreadManager::Initialize();
#endif
}

void ShowMissingLocaleMessageBox() {
  NOTIMPLEMENTED();
}
