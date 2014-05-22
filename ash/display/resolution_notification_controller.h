// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
#define ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/size.h"

namespace chromeos {
FORWARD_DECLARE_TEST(DisplayPreferencesTest, PreventStore);
}  // namespace chromeos

namespace views {
class Label;
class Widget;
}  // namespace views

namespace ash {

// A class which manages the notification of display resolution change and
// also manages the timeout in case the new resolution is unusable.
class ASH_EXPORT ResolutionNotificationController
    : public gfx::DisplayObserver,
      public DisplayController::Observer {
 public:
  ResolutionNotificationController();
  virtual ~ResolutionNotificationController();

  // Updates the display resolution for |display_id| to |new_resolution| and
  // creates a notification for this change which offers a button to revert the
  // change in case something goes wrong. The notification times out if there's
  // only one display connected and the user is trying to modify its resolution.
  // In that case, the timeout has to be set since the user cannot make any
  // changes if something goes wrong.
  void SetDisplayResolutionAndNotify(
      int64 display_id,
      const gfx::Size& old_resolution,
      const gfx::Size& new_resolution,
      const base::Closure& accept_callback);

  // Returns true if the notification is visible or scheduled to be visible and
  // the notification times out.
  bool DoesNotificationTimeout();

  // Called by the notification delegate when the user accepts the display
  // resolution change. Set |close_notification| to true when the notification
  // should be removed.
  void AcceptResolutionChange(bool close_notification);

  // Called by the notification delegate when the user wants to revert the
  // display resolution change.
  void RevertResolutionChange();

 private:
  friend class ResolutionNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(ResolutionNotificationControllerTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(chromeos::DisplayPreferencesTest, PreventStore);

  // A struct to bundle the data for a single resolution change.
  struct ResolutionChangeInfo;

  static const int kTimeoutInSec;
  static const char kNotificationId[];

  // Create a new notification, or update its content if it already exists.
  // |enable_spoken_feedback| is set to false when the notification is updated
  // during the countdown so the update isn't necessarily read by the spoken
  // feedback.
  void CreateOrUpdateNotification(bool enable_spoken_feedback);

  // Called every second for timeout.
  void OnTimerTick();

  // gfx::DisplayObserver overrides:
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  // DisplayController::Observer overrides:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  static void SuppressTimerForTest();

  scoped_ptr<ResolutionChangeInfo> change_info_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionNotificationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
