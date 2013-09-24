// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/fullscreen_notification_blocker.h"

#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/fullscreen.h"
#include "content/public/browser/notification_service.h"

#if defined(USE_ASH)
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

bool DoesFullscreenModeBlockNotifications() {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance()) {
    ash::internal::RootWindowController* controller =
        ash::internal::RootWindowController::ForTargetRootWindow();

    // During shutdown |controller| can be NULL.
    if (!controller)
      return false;

    const aura::Window* fullscreen_window =
        controller->GetTopmostFullscreenWindow();

    // Should appear notifications if kFullscreenUsesMinimalChromeKey is set,
    // since shelf/message_center UI is visible in such situation.
    return fullscreen_window && !fullscreen_window->GetProperty(
        ash::internal::kFullscreenUsesMinimalChromeKey);
  }
#endif

  return IsFullScreenMode();
}

}  // namespace

FullscreenNotificationBlocker::FullscreenNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center),
      is_fullscreen_mode_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                 content::NotificationService::AllSources());
}

FullscreenNotificationBlocker::~FullscreenNotificationBlocker() {
}

void FullscreenNotificationBlocker::CheckState() {
  bool was_fullscreen_mode = is_fullscreen_mode_;
  is_fullscreen_mode_ = DoesFullscreenModeBlockNotifications();
  if (is_fullscreen_mode_ != was_fullscreen_mode) {
    FOR_EACH_OBSERVER(
        NotificationBlocker::Observer, observers(), OnBlockingStateChanged());
  }
}

bool FullscreenNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::NotifierId& notifier_id) const {
  bool enabled = !is_fullscreen_mode_;
#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    enabled = enabled ||
        ash::system_notifier::ShouldAlwaysShowPopups(notifier_id);
#endif

  return enabled;
}

void FullscreenNotificationBlocker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  CheckState();
}
