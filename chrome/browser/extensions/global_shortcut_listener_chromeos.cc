// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_chromeos.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static GlobalShortcutListenerChromeOS* instance =
      new GlobalShortcutListenerChromeOS();
  return instance;
}

GlobalShortcutListenerChromeOS::GlobalShortcutListenerChromeOS()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListenerChromeOS::~GlobalShortcutListenerChromeOS() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerChromeOS::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  is_listening_ = true;
}

void GlobalShortcutListenerChromeOS::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  is_listening_ = false;
}

bool GlobalShortcutListenerChromeOS::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  ash::AcceleratorController* controller =
      ash::Shell::GetInstance()->accelerator_controller();
  if (controller->IsRegistered(accelerator))
    return false;

  // TODO(dtseng): Support search key mapping.
  controller->Register(accelerator, this);
  return controller->IsRegistered(accelerator);
}

void GlobalShortcutListenerChromeOS::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  // This code path gets called during object destruction.
  if (!ash::Shell::HasInstance())
    return;
  ash::Shell::GetInstance()->accelerator_controller()->Unregister(accelerator,
                                                                  this);
}

bool GlobalShortcutListenerChromeOS::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK(is_listening_);
  ash::AcceleratorController* controller =
      ash::Shell::GetInstance()->accelerator_controller();
  ash::AcceleratorController::AcceleratorProcessingRestriction restriction =
      controller->GetCurrentAcceleratorRestriction();
  if (restriction == ash::AcceleratorController::RESTRICTION_NONE) {
    NotifyKeyPressed(accelerator);
    return true;
  }
  return restriction == ash::AcceleratorController::
                            RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
}

bool GlobalShortcutListenerChromeOS::CanHandleAccelerators() const {
  return is_listening_;
}

}  // namespace extensions
