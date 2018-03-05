// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/upgrade_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

// A class that observes changes to the browser.relaunch_notification preference
// (which is backed by the RelaunchNotification policy setting) and annoyance
// levels from the UpgradeDetector. An appropriate notification is shown to the
// user based on the policy setting and current upgrade annoyance level.
// Notifications are shown at low, elevated, and high annoyance levels (as
// defined by the UpgradeDetector). In cases where an administrator recommends a
// relaunch, a bubble for such is repeatedly shown until the user relaunches.
class RelaunchNotificationController : public UpgradeObserver,
                                       public views::WidgetObserver {
 public:
  // |upgrade_detector| is expected to be the process-wide detector, and must
  // outlive the controller.
  explicit RelaunchNotificationController(UpgradeDetector* upgrade_detector);
  ~RelaunchNotificationController() override;

 protected:
  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  enum class NotificationStyle {
    kNone,         // No notifications are shown.
    kRecommended,  // Relaunches are recommended.
    kRequired,     // Relaunches are required.
  };

  // Returns the name of a histogram of |base_name| suffixed with the name of
  // the current notification style.
  std::string GetSuffixedHistogramName(base::StringPiece base_name);

  // Adjusts to the current notification style as indicated by the
  // browser.relaunch_notification Local State preference.
  void HandleCurrentStyle();

  // Bring the instance out of or back to dormant mode.
  void StartObservingUpgrades();
  void StopObservingUpgrades();

  // Shows the proper notification based on the preference setting. Invoked as a
  // result of a detected change in the UpgradeDetector's annoyance level to
  // |level|. In the case where a relaunch is recommended and |level| indicates
  // that the highest annoyance level has been reached, starts a timer to
  // repeatedly show the relaunch recommended notification.
  void ShowRelaunchNotification(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level);

  // Closes any previously-shown notifications. This is safe to call if no
  // notifications have been shown. Notifications may be closed by other means
  // (e.g., by the user), so there is no expectation that a previously-shown
  // notification is still open when this is invoked. In the case where a
  // relaunch is recommended, the timer to repeatedly show the relaunch
  // recommended notification is also stopped.
  void CloseRelaunchNotification();

  // Starts a timer to periodically re-show the relaunch recommended bubble.
  void StartReshowTimer();

  // Run on a timer once high annoyance has been reached to re-show the relaunch
  // recommended bubble.
  void OnReshowRelaunchRecommendedBubble();

  // The following methods, which are invoked by the controller to show or close
  // notifications, are virtual for the sake of testing.

  // Shows the relaunch recommended bubble if it is not already open.
  virtual void ShowRelaunchRecommendedBubble();

  // Shows the relaunch required dialog if it is not already open.
  virtual void ShowRelaunchRequiredDialog();

  // Closes the bubble or dialog if either is still open.
  virtual void CloseWidget();

  // Run to relaunch the browser once the relaunch deadline is reached when
  // relaunches are required by policy.
  virtual void OnRelaunchDeadlineExpired();

  // The process-wide upgrade detector.
  UpgradeDetector* const upgrade_detector_;

  // Observes changes to the browser.relaunch_notification Local State pref.
  PrefChangeRegistrar pref_change_registrar_;

  // The last observed notification style. When kNone, the controller is
  // said to be "dormant" as there is no work for it to do aside from watch for
  // changes to browser.relaunch_notification. When any other value, the
  // controller is observing the UpgradeDetector to detect when to show a
  // notification.
  NotificationStyle last_notification_style_;

  // The last observed annoyance level for which a notification was shown. This
  // member is unconditionally UPGRADE_ANNOYANCE_NONE when the controller is
  // dormant (browser.relaunch_notification is 0). It is any other value only
  // when a notification has been shown.
  UpgradeDetector::UpgradeNotificationAnnoyanceLevel last_level_;

  // The widget hosting the bubble or dialog, or null if neither is is currently
  // shown.
  views::Widget* widget_;

  // A timer used either to repeatedly reshow the relaunch recommended bubble
  // once the high annoyance level has been reached, or to trigger browser
  // relaunch once the relaunch required dialog's deadline is reached.
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_
