// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_

#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"
#include "components/content_settings/core/common/content_settings.h"

// This class implements mouselock behavior.
class MouseLockController : public ExclusiveAccessControllerBase {
 public:
  explicit MouseLockController(ExclusiveAccessManager* manager);
  ~MouseLockController() override;

  bool IsMouseLocked() const;
  bool IsMouseLockSilentlyAccepted() const;
  bool IsMouseLockRequested() const;

  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);

  // Override from ExclusiveAccessControllerBase
  bool HandleUserPressedEscape() override;

  void ExitExclusiveAccessToPreviousState() override;
  bool OnAcceptExclusiveAccessPermission() override;
  bool OnDenyExclusiveAccessPermission() override;

  // Called by Browser::LostMouseLock.
  void LostMouseLock();

  void UnlockMouse();

 private:
  enum MouseLockState {
    MOUSELOCK_NOT_REQUESTED,
    // The page requests to lock the mouse and the user hasn't responded to the
    // request.
    MOUSELOCK_REQUESTED,
    // Mouse lock has been allowed by the user.
    MOUSELOCK_ACCEPTED,
    // Mouse lock has been silently accepted, no notification to user.
    MOUSELOCK_ACCEPTED_SILENTLY
  };

  void NotifyMouseLockChange();

  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;

  ContentSetting GetMouseLockSetting(const GURL& url) const;

  MouseLockState mouse_lock_state_;

  DISALLOW_COPY_AND_ASSIGN(MouseLockController);
};

#endif  //  CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
