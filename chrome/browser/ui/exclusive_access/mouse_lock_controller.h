// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"
#include "components/content_settings/core/common/content_settings.h"

// This class implements mouselock behavior.
class MouseLockController : public ExclusiveAccessControllerBase {
 public:
  explicit MouseLockController(ExclusiveAccessManager* manager);
  ~MouseLockController() override;

  // Returns true if the mouse is locked.
  bool IsMouseLocked() const;

  // Returns true if the mouse was locked and no notification should be
  // displayed to the user. This is the case when a notice has already been
  // displayed to the user, and the application voluntarily unlocks, then
  // re-locks the mouse (a duplicate notification should not be given). See
  // content::MouseLockDispatcher::LockMouse.
  bool IsMouseLockedSilently() const;

  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);

  // Override from ExclusiveAccessControllerBase
  bool HandleUserPressedEscape() override;

  void ExitExclusiveAccessToPreviousState() override;

  // Called by Browser::LostMouseLock.
  void LostMouseLock();

  void UnlockMouse();

  // If true, does not call into the WebContents to lock the mouse. Just assumes
  // that it works. This may be necessary when calling
  // Browser::RequestToLockMouse in tests, because the proper signal will not
  // have been passed to the RenderViewHost.
  void set_fake_mouse_lock_for_test(bool value) {
    fake_mouse_lock_for_test_ = value;
  }

 private:
  enum MouseLockState {
    MOUSELOCK_UNLOCKED,
    // Mouse has been locked.
    MOUSELOCK_LOCKED,
    // Mouse has been locked silently, with no notification to user.
    MOUSELOCK_LOCKED_SILENTLY
  };

  void NotifyMouseLockChange();

  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;
  void RecordBubbleReshowsHistogram(int bubble_reshow_count) override;

  MouseLockState mouse_lock_state_;

  bool fake_mouse_lock_for_test_;

  DISALLOW_COPY_AND_ASSIGN(MouseLockController);
};

#endif  //  CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
