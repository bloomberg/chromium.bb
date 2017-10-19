// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
#define ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {
FORWARD_DECLARE_TEST(DisplayPreferencesTest, PreventStore);
}  // namespace chromeos

namespace ash {

// A class which manages the notification of display resolution change and
// also manages the timeout in case the new resolution is unusable.
class ASH_EXPORT ResolutionNotificationController
    : public display::DisplayObserver,
      public WindowTreeHostManager::Observer {
 public:
  ResolutionNotificationController();
  ~ResolutionNotificationController() override;

  // If |display_id| is not the internal display, Prepare a resolution change
  // notification for |display_id| from |old_resolution| to |new_resolution|,
  // which offers a button to revert the change in case something goes wrong.
  // The notification times out if there's only one display connected and the
  // user is trying to modify its resolution. In that case, the timeout has to
  // be set since the user cannot make any changes if something goes wrong.
  //
  // Then call DisplayManager::SetDisplayMode() to apply the resolution change,
  // and return the result; true if success, false otherwise.
  // In case SetDisplayMode() fails, the prepared notification will be
  // discarded.
  //
  // If |display_id| is the internal display, the resolution change is applied
  // directly without preparing the confirm/revert notification (this kind of
  // notification is only useful for external displays).
  //
  // This method does not create a notification itself. The notification will be
  // created the next OnDisplayConfigurationChanged(), which will be called
  // asynchronously after the resolution change is requested by this method.
  //
  // |accept_callback| will be called when the user accepts the resoltion change
  // by closing the notification bubble or clicking on the accept button (if
  // any).
  bool PrepareNotificationAndSetDisplayMode(
      int64_t display_id,
      const display::ManagedDisplayMode& old_resolution,
      const display::ManagedDisplayMode& new_resolution,
      const base::Closure& accept_callback) WARN_UNUSED_RESULT;

  // Returns true if the notification is visible or scheduled to be visible and
  // the notification times out.
  bool DoesNotificationTimeout();

  // Called by the notification delegate when the user accepts the display
  // resolution change. Set |close_notification| to true when the notification
  // should be removed.
  void AcceptResolutionChange(bool close_notification);

  // Called by the notification delegate when the user wants to revert the
  // display resolution change.
  void RevertResolutionChange(bool display_was_removed);

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

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // WindowTreeHostManager::Observer overrides:
  void OnDisplayConfigurationChanged() override;

  static void SuppressTimerForTest();

  std::unique_ptr<ResolutionChangeInfo> change_info_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionNotificationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
