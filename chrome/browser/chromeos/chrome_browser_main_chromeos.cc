// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/accessibility/system_event_observer.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_manager.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/brightness_observer.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/session_manager_observer.h"
#include "chrome/browser/chromeos/net/cros_network_change_notifier_factory.h"
#include "chrome/browser/chromeos/net/network_change_notifier_chromeos.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/main_function_params.h"
#include "net/base/network_change_notifier.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#if defined(USE_AURA)
#include "chrome/browser/chromeos/legacy_window_manager/initial_browser_window_observer.h"
#endif

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

static base::LazyInstance<MessageLoopObserver> g_message_loop_observer =
    LAZY_INSTANCE_INITIALIZER;

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsLinux(parameters) {
}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  chromeos::disks::DiskMountManager::Shutdown();

  chromeos::BluetoothManager::Shutdown();

  chromeos::DBusThreadManager::Shutdown();

  chromeos::accessibility::SystemEventObserver::Shutdown();

  if (!parameters().ui_task && chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Shutdown();

  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutDone",
                                                        false);
  chromeos::BootTimesLoader::Get()->WriteLogoutTimes();
}

void ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  ChromeBrowserMainPartsLinux::PreEarlyInitialization();
  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kDisableSync);
    singleton_command_line->AppendSwitch(switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  ChromeBrowserMainPartsLinux::PreMainMessageLoopStart();
  // Initialize CrosLibrary only for the browser, unless running tests
  // (which do their own CrosLibrary setup).
  if (!parameters().ui_task) {
    bool use_stub = parameters().command_line.HasSwitch(switches::kStubCros);
    chromeos::CrosLibrary::Initialize(use_stub);
  }
  // Replace the default NetworkChangeNotifierFactory with ChromeOS specific
  // implementation.
  net::NetworkChangeNotifier::SetFactory(
      new chromeos::CrosNetworkChangeNotifierFactory());

  chromeos::accessibility::SystemEventObserver::Initialize();
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
  // FILE thread is created in ChromeBrowserMainParts::PreMainMessageLoopRun().
  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
  // Get the statistics provider instance here to start loading statistcs
  // on the background FILE thread.
  chromeos::system::StatisticsProvider::GetInstance();

  // Initialize the Chrome OS bluetooth subsystem.
  // We defer this to PreMainMessageLoopRun because we don't want to check the
  // parsed command line until after about_flags::ConvertFlagsToSwitches has
  // been called.
  // TODO(vlaviano): Move this back to PostMainMessageLoopStart when we remove
  // the --enable-bluetooth flag.
  if (parsed_command_line().HasSwitch(switches::kEnableBluetooth)) {
    chromeos::BluetoothManager::Initialize();
  }
}

void ChromeBrowserMainPartsChromeos::PostMainMessageLoopStart() {
  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
  MessageLoopForUI* message_loop = MessageLoopForUI::current();
  message_loop->AddObserver(g_message_loop_observer.Pointer());

  // Initialize DBusThreadManager for the browser. This must be done after
  // the main message loop is started, as it uses the message loop.
  chromeos::DBusThreadManager::Initialize();

  // Initialize the brightness observer so that we'll display an onscreen
  // indication of brightness changes during login.
  brightness_observer_.reset(new chromeos::BrightnessObserver());
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      brightness_observer_.get());
  // Initialize the session manager observer so that we'll take actions
  // per signals sent from the session manager.
  session_manager_observer_.reset(new chromeos::SessionManagerObserver);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      AddObserver(session_manager_observer_.get());

  // Initialize the disk mount manager.
  chromeos::disks::DiskMountManager::Initialize();

  // Initialize the network change notifier for Chrome OS. The network
  // change notifier starts to monitor changes from the power manager and
  // the network manager.
  chromeos::CrosNetworkChangeNotifierFactory::GetInstance()->Init();

  // Likewise, initialize the upgrade detector for Chrome OS. The upgrade
  // detector starts to monitor changes from the update engine.
  UpgradeDetectorChromeos::GetInstance()->Init();

  if (chromeos::system::runtime_environment::IsRunningOnChromeOS()) {
    // For http://crosbug.com/p/5795 and http://crosbug.com/p/6245.
    // Enable Num Lock on X start up.
      chromeos::input_method::InputMethodManager::GetInstance()->
          GetXKeyboard()->SetNumLockEnabled(true);

#if defined(USE_AURA)
      initial_browser_window_observer_.reset(
          new chromeos::InitialBrowserWindowObserver);
#endif
  }
}

// Shut down services before the browser process, etc are destroyed.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  // Shutdown the upgrade detector for Chrome OS. The upgrade detector
  // stops monitoring changes from the update engine.
  if (UpgradeDetectorChromeos::GetInstance())
    UpgradeDetectorChromeos::GetInstance()->Shutdown();

  // Shutdown the network change notifier for Chrome OS. The network
  // change notifier stops monitoring changes from the power manager and
  // the network manager.
  if (chromeos::CrosNetworkChangeNotifierFactory::GetInstance())
    chromeos::CrosNetworkChangeNotifierFactory::GetInstance()->Shutdown();

  // We should remove observers attached to D-Bus clients before
  // DBusThreadManager is shut down.
  if (session_manager_observer_.get()) {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        RemoveObserver(session_manager_observer_.get());
  }
  if (brightness_observer_.get()) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()
        ->RemoveObserver(brightness_observer_.get());
  }
}
