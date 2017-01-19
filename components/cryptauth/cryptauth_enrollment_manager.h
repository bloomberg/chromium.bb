// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_MANAGER_H_
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/cryptauth/cryptauth_gcm_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/sync_scheduler.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class Time;
}

namespace cryptauth {

class CryptAuthEnroller;
class CryptAuthEnrollerFactory;
class SecureMessageDelegate;

// This class manages the device's enrollment with CryptAuth, periodically
// re-enrolling to keep the state on the server fresh. If an enrollment fails,
// the manager will schedule the next enrollment more aggressively to recover
// from the failure.
class CryptAuthEnrollmentManager : public SyncScheduler::Delegate,
                                   public CryptAuthGCMManager::Observer {
 public:
  class Observer {
   public:
    // Called when an enrollment attempt is started.
    virtual void OnEnrollmentStarted() = 0;

    // Called when an enrollment attempt finishes with the |success| of the
    // attempt.
    virtual void OnEnrollmentFinished(bool success) = 0;

    virtual ~Observer() {}
  };

  // Creates the manager:
  // |clock|: Used to determine the time between sync attempts.
  // |enroller_factory|: Creates CryptAuthEnroller instances to perform each
  //                     enrollment attempt.
  // |secure_message_delegate|: Used to generate the user's keypair if it does
  //                            not exist.
  // |device_info|: Contains information about the local device that will be
  //                uploaded to CryptAuth with each enrollment request.
  // |gcm_manager|: Used to perform GCM registrations and also notifies when GCM
  //                push messages trigger re-enrollments.
  //                Not owned and must outlive this instance.
  // |pref_service|: Contains preferences across browser restarts, and should
  //                 have been registered through RegisterPrefs().
  CryptAuthEnrollmentManager(
      std::unique_ptr<base::Clock> clock,
      std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
      std::unique_ptr<SecureMessageDelegate> secure_message_delegate,
      const GcmDeviceInfo& device_info,
      CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service);

  ~CryptAuthEnrollmentManager() override;

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Begins scheduling periodic enrollment attempts.
  void Start();

  // Adds an observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // Skips the waiting period and forces an enrollment immediately. If an
  // enrollment is already in progress, this function does nothing.
  // |invocation_reason| specifies the reason that the enrollment was triggered,
  // which is upload to the server.
  void ForceEnrollmentNow(InvocationReason invocation_reason);

  // Returns true if a successful enrollment has been recorded and this
  // enrollment has not expired.
  bool IsEnrollmentValid() const;

  // Returns the timestamp of the last successful enrollment. If no enrollment
  // has ever been made, then a null base::Time object will be returned.
  base::Time GetLastEnrollmentTime() const;

  // Returns the time to the next enrollment attempt.
  base::TimeDelta GetTimeToNextAttempt() const;

  // Returns true if an enrollment attempt is currently in progress.
  bool IsEnrollmentInProgress() const;

  // Returns true if the last enrollment failed and the manager is now
  // scheduling enrollments more aggressively to recover. If no enrollment has
  // ever been recorded, then this function will also return true.
  bool IsRecoveringFromFailure() const;

  // Returns the keypair used to enroll with CryptAuth. If no enrollment has
  // been completed, then an empty string will be returned.
  // Note: These keys are really serialized protocol buffer messages, and should
  // only be used by passing to SecureMessageDelegate.
  std::string GetUserPublicKey() const;
  std::string GetUserPrivateKey() const;

 protected:
  // Creates a new SyncScheduler instance. Exposed for testing.
  virtual std::unique_ptr<SyncScheduler> CreateSyncScheduler();

 private:
  // CryptAuthGCMManager::Observer:
  void OnGCMRegistrationResult(bool success) override;
  void OnReenrollMessage() override;

  // Callback when a new keypair is generated.
  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key);

  // SyncScheduler::Delegate:
  void OnSyncRequested(
      std::unique_ptr<SyncScheduler::SyncRequest> sync_request) override;

  // Starts a CryptAuth enrollment attempt, generating a new keypair if one is
  // not already stored in the user prefs.
  void DoCryptAuthEnrollment();

  // Starts a CryptAuth enrollment attempt, after a key-pair is stored in the
  // user prefs.
  void DoCryptAuthEnrollmentWithKeys();

  // Callback when |cryptauth_enroller_| completes.
  void OnEnrollmentFinished(bool success);

  // Used to determine the time.
  std::unique_ptr<base::Clock> clock_;

  // Creates CryptAuthEnroller instances for each enrollment attempt.
  std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory_;

  // The SecureMessageDelegate used to generate the user's keypair if it does
  // not already exist.
  std::unique_ptr<SecureMessageDelegate> secure_message_delegate_;

  // The local device information to upload to CryptAuth.
  const GcmDeviceInfo device_info_;

  //  Used to perform GCM registrations and also notifies when GCM push messages
  //  trigger re-enrollments. Not owned and must outlive this instance.
  CryptAuthGCMManager* gcm_manager_;

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts.
  // Not owned and must outlive this instance.
  PrefService* pref_service_;

  // Schedules the time between enrollment attempts.
  std::unique_ptr<SyncScheduler> scheduler_;

  // Contains the SyncRequest that |scheduler_| requests when an enrollment
  // attempt is made.
  std::unique_ptr<SyncScheduler::SyncRequest> sync_request_;

  // The CryptAuthEnroller instance for the current enrollment attempt. A new
  // instance will be created for each individual attempt.
  std::unique_ptr<CryptAuthEnroller> cryptauth_enroller_;

  // List of observers.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CryptAuthEnrollmentManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollmentManager);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_MANAGER_H_
