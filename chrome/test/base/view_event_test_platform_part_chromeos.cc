// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_platform_part.h"

#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/wm_state.h"

namespace {

// ViewEventTestPlatformPart implementation for ChromeOS (chromeos=1).
class ViewEventTestPlatformPartChromeOS : public ViewEventTestPlatformPart {
 public:
  explicit ViewEventTestPlatformPartChromeOS(
      ui::ContextFactory* context_factory);
  virtual ~ViewEventTestPlatformPartChromeOS();

  // Overridden from ViewEventTestPlatformPart:
  virtual gfx::NativeWindow GetContext() OVERRIDE {
    return ash::Shell::GetPrimaryRootWindow();
  }

 private:
  wm::WMState wm_state_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartChromeOS);
};

ViewEventTestPlatformPartChromeOS::ViewEventTestPlatformPartChromeOS(
    ui::ContextFactory* context_factory) {
  // Ash Shell can't just live on its own without a browser process, we need to
  // also create the message center.
  message_center::MessageCenter::Initialize();
  chromeos::DBusThreadManager::Initialize();
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::NetworkHandler::Initialize();
  ash::test::TestShellDelegate* shell_delegate =
      new ash::test::TestShellDelegate();
  ash::ShellInitParams init_params;
  init_params.delegate = shell_delegate;
  init_params.context_factory = context_factory;
  ash::Shell::CreateInstance(init_params);
  shell_delegate->test_session_state_delegate()->SetActiveUserSessionStarted(
      true);
  GetContext()->GetHost()->Show();
}

ViewEventTestPlatformPartChromeOS::~ViewEventTestPlatformPartChromeOS() {
  ash::Shell::DeleteInstance();
  chromeos::NetworkHandler::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  // Ash Shell can't just live on its own without a browser process, we need to
  // also shut down the message center.
  message_center::MessageCenter::Shutdown();

  aura::Env::DeleteInstance();
}

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory) {
  return new ViewEventTestPlatformPartChromeOS(context_factory);
}
