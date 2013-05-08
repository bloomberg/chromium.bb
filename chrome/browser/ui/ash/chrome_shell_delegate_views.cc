// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/magnifier/magnifier_constants.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_views.h"
#include "chrome/browser/ui/ash/window_positioner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

void ChromeShellDelegate::PreInit() {
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

void ChromeShellDelegate::SilenceSpokenFeedback() const {
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
  switch (type) {
    case chrome::NOTIFICATION_ASH_SESSION_STARTED:
      GetTargetBrowser()->window()->Show();
      break;
    case chrome::NOTIFICATION_ASH_SESSION_ENDED:
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void ChromeShellDelegate::PlatformInit() {
#if defined(OS_WIN)
  registrar_.Add(this,
                 chrome::NOTIFICATION_ASH_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ASH_SESSION_ENDED,
                 content::NotificationService::AllSources());
#endif
}
