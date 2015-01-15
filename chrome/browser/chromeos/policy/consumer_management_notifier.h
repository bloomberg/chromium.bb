// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace policy {

class ConsumerManagementStage;

// ConsumerManagementNotifier observes ConsumerManagementService and shows
// a desktop notification to the device owner if consumer enrollment or
// unenrollment suceeds or fails.
class ConsumerManagementNotifier
    : public KeyedService,
      public ConsumerManagementService::Observer {
 public:
  ConsumerManagementNotifier(
      Profile* profile,
      ConsumerManagementService* consumer_management_service);
  ~ConsumerManagementNotifier() override;

  void Shutdown() override;

  // ConsumerManagementService::Observer:
  void OnConsumerManagementStatusChanged() override;

 private:
  // Shows a desktop notification.
  void ShowDesktopNotification(const ConsumerManagementStage& stage);

  // Opens the settings page.
  void OpenSettingsPage() const;

  // Opens the enrollment confirmation dialog in the settings page.
  void TryEnrollmentAgain() const;

  Profile* profile_;
  ConsumerManagementService* consumer_management_service_;

  base::WeakPtrFactory<ConsumerManagementNotifier> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementNotifier);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_H_
