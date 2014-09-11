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
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"

class PrefRegistrySimple;
class Profile;

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
class ConsumerManagementService
    : public chromeos::DeviceSettingsService::Observer,
      public content::NotificationObserver,
      public OAuth2TokenService::Consumer,
      public OAuth2TokenService::Observer {
 public:
  // The status indicates if the device is enrolled, or if enrollment or
  // unenrollment is in progress. If you want to add a value here, please also
  // update |kStatusString| in the .cc file, and |ConsumerManagementStatus| in
  // chrome/browser/resources/options/chromeos/consumer_management_overlay.js
  enum Status {
    // The status is currently unavailable.
    STATUS_UNKNOWN = 0,

    STATUS_ENROLLED,
    STATUS_ENROLLING,
    STATUS_UNENROLLED,
    STATUS_UNENROLLING,

    // This should always be the last one.
    STATUS_LAST,
  };

  // Indicating which stage the enrollment process is in.
  enum EnrollmentStage {
    // Not enrolled, or enrollment is completed.
    ENROLLMENT_STAGE_NONE = 0,
    // Enrollment is requested by the owner.
    ENROLLMENT_STAGE_REQUESTED,
    // The owner ID is stored in the boot lockbox.
    ENROLLMENT_STAGE_OWNER_STORED,
    // Success. The notification is not sent yet.
    ENROLLMENT_STAGE_SUCCESS,

    // Error stages.
    // Canceled by the user.
    ENROLLMENT_STAGE_CANCELED,
    // Failed to write to the boot lockbox.
    ENROLLMENT_STAGE_BOOT_LOCKBOX_FAILED,
    // Failed to get the access token.
    ENROLLMENT_STAGE_GET_TOKEN_FAILED,
    // Failed to register the device.
    ENROLLMENT_STAGE_DM_SERVER_FAILED,

    // This should always be the last one.
    ENROLLMENT_STAGE_LAST,
  };

  class Observer {
   public:
    // Called when the status changes.
    virtual void OnConsumerManagementStatusChanged() = 0;
  };

  // GetOwner() invokes this with an argument set to the owner user ID,
  // or an empty string on failure.
  typedef base::Callback<void(const std::string&)> GetOwnerCallback;

  // SetOwner() invokes this with an argument indicating success or failure.
  typedef base::Callback<void(bool)> SetOwnerCallback;

  // |client| and |device_settings_service| should outlive this object.
  ConsumerManagementService(
      chromeos::CryptohomeClient* client,
      chromeos::DeviceSettingsService* device_settings_service);

  virtual ~ConsumerManagementService();

  // Registers prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the status.
  Status GetStatus() const;

  // Returns the string value of the status.
  std::string GetStatusString() const;

  // Returns the enrollment stage.
  EnrollmentStage GetEnrollmentStage() const;

  // Sets the enrollment stage.
  void SetEnrollmentStage(EnrollmentStage stage);

  // Returns the device owner stored in the boot lockbox via |callback|.
  void GetOwner(const GetOwnerCallback& callback);

  // Stores the device owner user ID into the boot lockbox and signs it.
  // |callback| is invoked with an agument indicating success or failure.
  void SetOwner(const std::string& user_id, const SetOwnerCallback& callback);

  // chromeos::DeviceSettingsService::Observer:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

  // content::NotificationObserver implmentation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // OAuth2TokenService::Consumer:
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
  void EndEnrollment(EnrollmentStage stage);

  // Shows a desktop notification and resets the enrollment stage.
  void ShowDesktopNotificationAndResetStage(
      EnrollmentStage stage,
      Profile* profile);

  // Opens the settings page.
  void OpenSettingsPage(Profile* profile) const;

  // Opens the enrollment confirmation dialog in the settings page.
  void TryEnrollmentAgain(Profile* profile) const;

  void NotifyStatusChanged();

  chromeos::CryptohomeClient* client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  Profile* enrolling_profile_;
  scoped_ptr<OAuth2TokenService::Request> token_request_;
  content::NotificationRegistrar registrar_;
  ObserverList<Observer, true> observers_;
  base::WeakPtrFactory<ConsumerManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
