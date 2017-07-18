// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_platform_part.h"

#include <memory>

#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_switches.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/wm_state.h"

namespace {

// ViewEventTestPlatformPart implementation for ChromeOS (chromeos=1).
class ViewEventTestPlatformPartChromeOS : public ViewEventTestPlatformPart {
 public:
  ViewEventTestPlatformPartChromeOS(
      ui::ContextFactory* context_factory,
      ui::ContextFactoryPrivate* context_factory_private);
  ~ViewEventTestPlatformPartChromeOS() override;

  // Overridden from ViewEventTestPlatformPart:
  gfx::NativeWindow GetContext() override {
    return ash::Shell::GetPrimaryRootWindow();
  }

 private:
  wm::WMState wm_state_;
  std::unique_ptr<aura::Env> env_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartChromeOS);
};

ViewEventTestPlatformPartChromeOS::ViewEventTestPlatformPartChromeOS(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  // Ash Shell can't just live on its own without a browser process, we need to
  // also create the message center.
  message_center::MessageCenter::Initialize();
  chromeos::DBusThreadManager::Initialize();
  bluez::BluezDBusManager::Initialize(
      chromeos::DBusThreadManager::Get()->GetSystemBus(),
      chromeos::DBusThreadManager::Get()->IsUsingFakes());
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::NetworkHandler::Initialize();

  env_ = aura::Env::CreateInstance();
  ash::test::TestShellDelegate* shell_delegate =
      new ash::test::TestShellDelegate();
  ash::ShellInitParams init_params;
  init_params.delegate = shell_delegate;
  init_params.context_factory = context_factory;
  init_params.context_factory_private = context_factory_private;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHostWindowBounds, "0+0-1280x800");
  ash::Shell::CreateInstance(init_params);
  ash::test::TestSessionControllerClient session_controller_client(
      ash::Shell::Get()->session_controller());
  session_controller_client.CreatePredefinedUserSessions(1);
  GetContext()->GetHost()->Show();
}

ViewEventTestPlatformPartChromeOS::~ViewEventTestPlatformPartChromeOS() {
  ash::Shell::DeleteInstance();
  env_.reset();

  chromeos::NetworkHandler::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  // Ash Shell can't just live on its own without a browser process, we need to
  // also shut down the message center.
  message_center::MessageCenter::Shutdown();
}

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new ViewEventTestPlatformPartChromeOS(context_factory,
                                               context_factory_private);
}
