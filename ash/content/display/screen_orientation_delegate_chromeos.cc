// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/display/screen_orientation_delegate_chromeos.h"

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "content/public/browser/screen_orientation_provider.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/size.h"

namespace {

blink::WebScreenOrientationLockType GetDisplayNaturalOrientation() {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return blink::WebScreenOrientationLockLandscape;

  ash::DisplayInfo info =
      display_manager->GetDisplayInfo(gfx::Display::InternalDisplayId());
  gfx::Size size = info.size_in_pixel();
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
    case gfx::Display::ROTATE_180:
      return size.height() >= size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
    case gfx::Display::ROTATE_90:
    case gfx::Display::ROTATE_270:
      return size.height() < size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
  }
  NOTREACHED();
  return blink::WebScreenOrientationLockLandscape;
}

}  // namespace

namespace ash {

ScreenOrientationDelegate::ScreenOrientationDelegate()
    : locking_window_(NULL),
      natural_orientation_(GetDisplayNaturalOrientation()) {
  content::ScreenOrientationProvider::SetDelegate(this);
}

ScreenOrientationDelegate::~ScreenOrientationDelegate() {
  content::ScreenOrientationProvider::SetDelegate(NULL);
}

bool ScreenOrientationDelegate::FullScreenRequired(
    content::WebContents* web_contents) {
  return true;
}

void ScreenOrientationDelegate::Lock(
    content::WebContents* web_contents,
    blink::WebScreenOrientationLockType lock_orientation) {
  aura::Window* requesting_window = web_contents->GetNativeView();

  // TODO(jonross): Make ScreenOrientationDelegate responsible for rotation
  // lock. Have MaximizeModeController, and TrayRotationLock both use it
  // instead.
  MaximizeModeController* controller =
      Shell::GetInstance()->maximize_mode_controller();

  // TODO(jonross): Track one rotation lock per window. When the active window
  // changes apply any corresponding rotation lock.
  if (!locking_window_)
    locking_window_ = requesting_window;
  else if (requesting_window != locking_window_)
    return;

  switch (lock_orientation) {
    case blink::WebScreenOrientationLockAny:
      controller->SetRotationLocked(false);
      locking_window_ = NULL;
      break;
    case blink::WebScreenOrientationLockDefault:
      NOTREACHED();
      break;
    case blink::WebScreenOrientationLockPortraitPrimary:
      LockRotationToPrimaryOrientation(blink::WebScreenOrientationLockPortrait);
      break;
    case blink::WebScreenOrientationLockLandscape:
    case blink::WebScreenOrientationLockPortrait:
      LockToRotationMatchingOrientation(lock_orientation);
      break;
    case blink::WebScreenOrientationLockPortraitSecondary:
      LockRotationToSecondaryOrientation(
          blink::WebScreenOrientationLockPortrait);
      break;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      LockRotationToSecondaryOrientation(
          blink::WebScreenOrientationLockLandscape);
      break;
    case blink::WebScreenOrientationLockLandscapePrimary:
      LockRotationToPrimaryOrientation(
          blink::WebScreenOrientationLockLandscape);
      break;
    case blink::WebScreenOrientationLockNatural:
      controller->LockRotation(gfx::Display::ROTATE_0);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool ScreenOrientationDelegate::ScreenOrientationProviderSupported() {
  return Shell::GetInstance()
             ->maximize_mode_controller()
             ->IsMaximizeModeWindowManagerEnabled();
}

void ScreenOrientationDelegate::Unlock(content::WebContents* web_contents) {
  aura::Window* requesting_window = web_contents->GetNativeView();
  if (requesting_window != locking_window_)
    return;
  locking_window_ = NULL;
  Shell::GetInstance()->maximize_mode_controller()->SetRotationLocked(false);
}

void ScreenOrientationDelegate::LockRotationToPrimaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  Shell::GetInstance()->maximize_mode_controller()->LockRotation(
      natural_orientation_ == lock_orientation ? gfx::Display::ROTATE_0
                                               : gfx::Display::ROTATE_90);
}

void ScreenOrientationDelegate::LockRotationToSecondaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  Shell::GetInstance()->maximize_mode_controller()->LockRotation(
      natural_orientation_ == lock_orientation ? gfx::Display::ROTATE_180
                                               : gfx::Display::ROTATE_270);
}

void ScreenOrientationDelegate::LockToRotationMatchingOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  // TODO(jonross): Update MaximizeModeController to allow rotation between
  // two angles of an orientation (e.g. from ROTATE_0 to ROTATE_180, and from
  // ROTATE_90 to ROTATE_270)
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return;

  gfx::Display::Rotation rotation =
      display_manager->GetDisplayInfo(gfx::Display::InternalDisplayId())
          .rotation();
  MaximizeModeController* controller =
      Shell::GetInstance()->maximize_mode_controller();
  if (natural_orientation_ == lock_orientation) {
    if (rotation == gfx::Display::ROTATE_0 ||
        rotation == gfx::Display::ROTATE_180) {
      controller->SetRotationLocked(true);
    } else {
      controller->LockRotation(gfx::Display::ROTATE_0);
    }
  } else {
    if (rotation == gfx::Display::ROTATE_90 ||
        rotation == gfx::Display::ROTATE_270) {
      controller->SetRotationLocked(true);
    } else {
      controller->LockRotation(gfx::Display::ROTATE_90);
    }
  }
}

}  // namespace ash
