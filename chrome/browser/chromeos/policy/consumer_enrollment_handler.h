// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

namespace policy {

class DeviceManagementService;
class EnrollmentStatus;

// Consumer enrollment handler automatically continues the enrollment process
// after the owner ID is stored in the boot lockbox and thw owner signs in.
class ConsumerEnrollmentHandler
    : public KeyedService,
      public OAuth2TokenService::Consumer,
      public OAuth2TokenService::Observer {
 public:
  ConsumerEnrollmentHandler(
      Profile* profile,
      ConsumerManagementService* consumer_management_service,
      DeviceManagementService* device_management_service);
  virtual ~ConsumerEnrollmentHandler();

  virtual void Shutdown() override;

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) override;

  // OAuth2TokenService::Consumer:
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) override;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) override;

  OAuth2TokenService::Request* GetTokenRequestForTesting() {
    return token_request_.get();
  }

 private:
  void Start();

  // Continues the enrollment process after the owner ID is stored into the boot
  // lockbox and the owner signs in.
  void ContinueEnrollmentProcess();

  // Called when the owner's refresh token is available.
  void OnOwnerRefreshTokenAvailable();

  // Called when the owner's access token for device management is available.
  void OnOwnerAccessTokenAvailable(const std::string& access_token);

  // Called upon the completion of the enrollment process.
  void OnEnrollmentCompleted(EnrollmentStatus status);

  // Ends the enrollment process and shows a desktop notification if the
  // current user is the owner.
  void EndEnrollment(ConsumerManagementService::EnrollmentStage stage);

  // Shows a desktop notification and resets the enrollment stage.
  void ShowDesktopNotificationAndResetStage(
      ConsumerManagementService::EnrollmentStage stage);

  // Opens the settings page.
  void OpenSettingsPage() const;

  // Opens the enrollment confirmation dialog in the settings page.
  void TryEnrollmentAgain() const;

  Profile* profile_;
  ConsumerManagementService* consumer_management_service_;
  DeviceManagementService* device_management_service_;
  std::string gaia_account_id_;

  scoped_ptr<OAuth2TokenService::Request> token_request_;
  base::WeakPtrFactory<ConsumerEnrollmentHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerEnrollmentHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_H_
