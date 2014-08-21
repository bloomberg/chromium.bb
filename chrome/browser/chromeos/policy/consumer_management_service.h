// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"

class PrefRegistrySimple;
class Profile;
class ProfileOAuth2TokenService;

namespace chromeos {
class CryptohomeClient;
}

namespace cryptohome {
class BaseReply;
}

namespace policy {

class EnrollmentStatus;

// The consumer management service handles several things:
//
// 1. The consumer enrollment state: The consumer enrollment state is an enum
//    value stored in local state to pass the information across reboots and
//    between components, including settings page, sign-in screen, and user
//    notification.
//
// 2. Boot lockbox owner ID: Unlike the owner ID in CrosSettings, the owner ID
//    stored in the boot lockbox can only be modified after reboot and before
//    the first session starts. It is guaranteed that if the device is consumer
//    managed, the owner ID in the boot lockbox will be available, but not the
//    other way.
//
// 3. Consumer management enrollment process: The service kicks off the last
//    part of the consumer management enrollment process after the owner ID is
//    stored in the boot lockbox and the owner signs in.
class ConsumerManagementService : public content::NotificationObserver,
                                  public OAuth2TokenService::Consumer,
                                  public OAuth2TokenService::Observer {
 public:
  enum ConsumerEnrollmentState {
    ENROLLMENT_NONE = 0,        // Not enrolled, or enrollment is completed.
    ENROLLMENT_REQUESTED,       // Enrollment is requested by the owner.
    ENROLLMENT_OWNER_STORED,    // The owner ID is stored in the boot lockbox.
    ENROLLMENT_SUCCESS,         // Success. The notification is not sent yet.

    // Error states.
    ENROLLMENT_CANCELED,             // Canceled by the user.
    ENROLLMENT_BOOT_LOCKBOX_FAILED,  // Failed to write to the boot lockbox.
    ENROLLMENT_GET_TOKEN_FAILED,     // Failed to get the access token.
    ENROLLMENT_DM_SERVER_FAILED,     // Failed to register the device.

    ENROLLMENT_LAST,            // This should always be the last one.
  };

  // GetOwner() invokes this with an argument set to the owner user ID,
  // or an empty string on failure.
  typedef base::Callback<void(const std::string&)> GetOwnerCallback;

  // SetOwner() invokes this with an argument indicating success or failure.
  typedef base::Callback<void(bool)> SetOwnerCallback;

  explicit ConsumerManagementService(chromeos::CryptohomeClient* client);

  virtual ~ConsumerManagementService();

  // Registers prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the enrollment state.
  ConsumerEnrollmentState GetEnrollmentState() const;

  // Sets the enrollment state.
  void SetEnrollmentState(ConsumerEnrollmentState state);

  // Returns the device owner stored in the boot lockbox via |callback|.
  void GetOwner(const GetOwnerCallback& callback);

  // Stores the device owner user ID into the boot lockbox and signs it.
  // |callback| is invoked with an agument indicating success or failure.
  void SetOwner(const std::string& user_id, const SetOwnerCallback& callback);

  // content::NotificationObserver implmentation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

  OAuth2TokenService::Request* GetTokenRequestForTesting() {
    return token_request_.get();
  }

 private:
  void OnGetBootAttributeDone(
      const GetOwnerCallback& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool dbus_success,
      const cryptohome::BaseReply& reply);

  void OnSetBootAttributeDone(const SetOwnerCallback& callback,
                              chromeos::DBusMethodCallStatus call_status,
                              bool dbus_success,
                              const cryptohome::BaseReply& reply);

  void OnFlushAndSignBootAttributesDone(
      const SetOwnerCallback& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool dbus_success,
      const cryptohome::BaseReply& reply);

  // Called when the owner signs in.
  void OnOwnerSignin(Profile* profile);

  // Continues the enrollment process after the owner ID is stored into the boot
  // lockbox and the owner signs in.
  void ContinueEnrollmentProcess(Profile* profile);

  // Called when the owner's refresh token is available.
  void OnOwnerRefreshTokenAvailable();

  // Called when the owner's access token for device management is available.
  void OnOwnerAccessTokenAvailable(const std::string& access_token);

  // Called upon the completion of the enrollment process.
  void OnEnrollmentCompleted(EnrollmentStatus status);

  // Ends the enrollment process and shows a desktop notification if the
  // current user is the owner.
  void EndEnrollment(ConsumerEnrollmentState state);

  // Shows a desktop notification and resets the enrollment state.
  void ShowDesktopNotificationAndResetState(ConsumerEnrollmentState state);

  chromeos::CryptohomeClient* client_;

  std::string enrolling_account_id_;
  ProfileOAuth2TokenService* enrolling_token_service_;

  scoped_ptr<OAuth2TokenService::Request> token_request_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<ConsumerManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
