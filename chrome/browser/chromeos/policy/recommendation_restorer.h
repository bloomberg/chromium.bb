// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/wm/core/user_activity_observer.h"

class Profile;

namespace policy {

// Observes a set of prefs in the login profile. If any of the prefs has a
// recommended value, its user setting is cleared so that the recommendation can
// take effect. This happens immediately when the login screen is shown, when
// a session is being started and whenever recommended values change during a
// user session. On the login screen, user settings are cleared when the user
// becomes idle for one minute.
class RecommendationRestorer : public KeyedService,
                               public content::NotificationObserver,
                               public wm::UserActivityObserver {
 public:
  explicit RecommendationRestorer(Profile* profile);
  virtual ~RecommendationRestorer();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // wm::UserActivityObserver:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // If a recommended value and a user setting exist for |pref_name|, clears the
  // user setting so that the recommended value can take effect. If
  // |allow_delay| is |true| and the login screen is being shown, a timer is
  // started that will clear the setting when the user becomes idle for one
  // minute. Otherwise, the setting is cleared immediately.
  void Restore(bool allow_delay, const std::string& pref_name);

 private:
  friend class RecommendationRestorerTest;

  void RestoreAll();

  void StartTimer();
  void StopTimer();

  PrefChangeRegistrar pref_change_registrar_;
  content::NotificationRegistrar notification_registrar_;

  bool logged_in_;

  base::OneShotTimer<RecommendationRestorer> restore_timer_;

  DISALLOW_COPY_AND_ASSIGN(RecommendationRestorer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_H_
