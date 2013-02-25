// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/magnifier/magnifier_constants.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_views.h"
#include "chrome/browser/ui/ash/window_positioner.h"

bool ChromeShellDelegate::IsUserLoggedIn() const {
  return true;
}

// Returns true if we're logged in and browser has been started
bool ChromeShellDelegate::IsSessionStarted() const {
  return true;
}

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

bool ChromeShellDelegate::CanLockScreen() const {
  return false;
}

void ChromeShellDelegate::LockScreen() {
}

bool ChromeShellDelegate::IsScreenLocked() const {
  return false;
}

void ChromeShellDelegate::Shutdown() {
}

void ChromeShellDelegate::OpenFileManager(bool as_dialog) {
}

void ChromeShellDelegate::OpenCrosh() {
}

void ChromeShellDelegate::OpenMobileSetup(const std::string& service_path) {
}

void ChromeShellDelegate::ShowKeyboardOverlay() {
}

void ChromeShellDelegate::ToggleHighContrast() {
}

bool ChromeShellDelegate::IsSpokenFeedbackEnabled() const {
  return false;
}

void ChromeShellDelegate::ToggleSpokenFeedback(
    ash::AccessibilityNotificationVisibility notify) {
}

bool ChromeShellDelegate::IsHighContrastEnabled() const {
  return false;
}

void ChromeShellDelegate::SetMagnifierEnabled(bool enabled) {
}

void ChromeShellDelegate::SetMagnifierType(ash::MagnifierType type) {
}

bool ChromeShellDelegate::IsMagnifierEnabled() const {
  return false;
}

ash::MagnifierType ChromeShellDelegate::GetMagnifierType() const {
  return ash::kDefaultMagnifierType;
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  return new CapsLockDelegate();
}

void ChromeShellDelegate::SaveScreenMagnifierScale(double scale) {
}

double ChromeShellDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

bool ChromeShellDelegate::ShouldAlwaysShowAccessibilityMenu() const {
  return false;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return NULL;
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return NULL;
}

void ChromeShellDelegate::HandleMediaNextTrack() {
}

void ChromeShellDelegate::HandleMediaPlayPause() {
}

void ChromeShellDelegate::HandleMediaPrevTrack() {
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  // MSVC++ warns about switch statements without any cases.
  NOTREACHED() << "Unexpected notification " << type;
}

void ChromeShellDelegate::PlatformInit() {
}
