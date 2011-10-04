// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/net/cros_network_change_notifier_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/main_function_params.h"
#include "net/base/network_change_notifier.h"

#include <gtk/gtk.h>

class MessageLoopObserver : public MessageLoopForUI::Observer {
#if defined(TOUCH_UI) || defined(USE_AURA)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
  }
#else
  virtual void WillProcessEvent(GdkEvent* event) {
    // On chromeos we want to map Alt-left click to right click.
    // This code only changes presses and releases. We could decide to also
    // modify drags and crossings. It doesn't seem to be a problem right now
    // with our support for context menus (the only real need we have).
    // There are some inconsistent states both with what we have and what
    // we would get if we added drags. You could get a right drag without a
    // right down for example, unless we started synthesizing events, which
    // seems like more trouble than it's worth.
    if ((event->type == GDK_BUTTON_PRESS ||
        event->type == GDK_2BUTTON_PRESS ||
        event->type == GDK_3BUTTON_PRESS ||
        event->type == GDK_BUTTON_RELEASE) &&
        event->button.button == 1 &&
        event->button.state & GDK_MOD1_MASK) {
      // Change the button to the third (right) one.
      event->button.button = 3;
      // Remove the Alt key and first button state.
      event->button.state &= ~(GDK_MOD1_MASK | GDK_BUTTON1_MASK);
      // Add the third (right) button state.
      event->button.state |= GDK_BUTTON3_MASK;
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
  }
#endif
};

static base::LazyInstance<MessageLoopObserver> g_message_loop_observer(
    base::LINKER_INITIALIZED);

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const MainFunctionParams& parameters)
    : ChromeBrowserMainPartsGtk(parameters) {
}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  chromeos::DBusThreadManager::Shutdown();

  if (!parameters().ui_task && chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Shutdown();

  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutDone",
                                                        false);
  chromeos::BootTimesLoader::Get()->WriteLogoutTimes();
}

void ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  ChromeBrowserMainPartsGtk::PreEarlyInitialization();
  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kDisableSync);
    singleton_command_line->AppendSwitch(switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  ChromeBrowserMainPartsGtk::PreMainMessageLoopStart();
  // Initialize CrosLibrary only for the browser, unless running tests
  // (which do their own CrosLibrary setup).
  if (!parameters().ui_task) {
    bool use_stub = parameters().command_line_.HasSwitch(switches::kStubCros);
    chromeos::CrosLibrary::Initialize(use_stub);
  }
  // Replace the default NetworkChangeNotifierFactory with ChromeOS specific
  // implementation.
  net::NetworkChangeNotifier::SetFactory(
      new chromeos::CrosNetworkChangeNotifierFactory());
}

void ChromeBrowserMainPartsChromeos::PostMainMessageLoopStart() {
  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();
  MessageLoopForUI* message_loop = MessageLoopForUI::current();
  message_loop->AddObserver(g_message_loop_observer.Pointer());

  // Initialize DBusThreadManager for the browser. This must be done after
  // the main message loop is started, as it uses the message loop.
  chromeos::DBusThreadManager::Initialize();
}
