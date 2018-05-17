// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefRegistrySimple;
class Profile;

namespace content {
class NotificationDetails;
class NotificationSource;
}  // namespace content

namespace policy {

// Observes the pref that indicates whether to log events for app push-installs.
// When logging is enabled, creates an |AppInstallEventLogManager|. When logging
// is disabled, destroys the |AppInstallEventLogManager|, if any, and clears all
// data related to the app-install event log. Ensures correct sequencing of I/O
// operations by using one |AppInstallEventLogManager::LogTaskRunnerWrapper| for
// all accesses to the log file.
class AppInstallEventLogManagerWrapper : public content::NotificationObserver {
 public:
  ~AppInstallEventLogManagerWrapper() override;

  // Creates a new |AppInstallEventLogManager| to handle app push-install event
  // logging for |profile|. The object returned manages its own lifetime and
  // self-destructs on logout.
  static AppInstallEventLogManagerWrapper* CreateForProfile(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  explicit AppInstallEventLogManagerWrapper(Profile* profile);

  // Must be called right after construction. Extracted into a separate method
  // for testing.
  void Init();

  // Creates the |log_manager_|. Virtual for testing.
  virtual void CreateManager();

  // Destroys the |log_manager_|. Virtual for testing.
  virtual void DestroyManager();

  // Provides the task runner used for all I/O on the log file.
  std::unique_ptr<AppInstallEventLogManager::LogTaskRunnerWrapper>
      log_task_runner_;

 private:
  // Evaluates the current state of the pref that indicates whether to log
  // events for app push-installs. If logging is enabled, creates the
  // |log_manager_|. If logging is disabled, destroys the |log_manager_| and
  // clears all data related to the app-install event log.
  void EvaluatePref();

  // The profile whose app push-install events are being logged.
  Profile* const profile_;

  // Handles collection, storage and upload of app push-install event logs.
  std::unique_ptr<AppInstallEventLogManager> log_manager_;

  // Pref change observer.
  PrefChangeRegistrar pref_change_registrar_;

  // Notification observer, used to destroy the |log_manager_| when the user is
  // logging out, giving it an opportunity to log the event.
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogManagerWrapper);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
