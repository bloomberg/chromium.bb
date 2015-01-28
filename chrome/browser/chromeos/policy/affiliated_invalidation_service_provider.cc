// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/ticl_device_settings_provider.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_identity_provider.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_content_client.h"
#include "components/invalidation/invalidation_handler.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator_state.h"
#include "components/invalidation/invalidator_storage.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/ticl_invalidation_service.h"
#include "components/invalidation/ticl_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/identity_provider.h"

namespace policy {

AffiliatedInvalidationServiceProvider::Consumer::~Consumer() {
}

class AffiliatedInvalidationServiceProvider::InvalidationServiceObserver
    : public syncer::InvalidationHandler {
 public:
  explicit InvalidationServiceObserver(
      AffiliatedInvalidationServiceProvider* parent,
      invalidation::InvalidationService* invalidation_service);
  ~InvalidationServiceObserver() override;

  invalidation::InvalidationService* GetInvalidationService();
  bool IsServiceConnected() const;

  // public syncer::InvalidationHandler:
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 private:
  AffiliatedInvalidationServiceProvider* parent_;
  invalidation::InvalidationService* invalidation_service_;
  bool is_service_connected_;
  bool is_observer_ready_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceObserver);
};

AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    InvalidationServiceObserver(
        AffiliatedInvalidationServiceProvider* parent,
        invalidation::InvalidationService* invalidation_service)
    : parent_(parent),
      invalidation_service_(invalidation_service),
      is_service_connected_(false),
      is_observer_ready_(false) {
  invalidation_service_->RegisterInvalidationHandler(this);
  is_service_connected_ = invalidation_service->GetInvalidatorState() ==
      syncer::INVALIDATIONS_ENABLED;
  is_observer_ready_ = true;
}

AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    ~InvalidationServiceObserver() {
  is_observer_ready_ = false;
  invalidation_service_->UnregisterInvalidationHandler(this);
}

invalidation::InvalidationService*
AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    GetInvalidationService() {
  return invalidation_service_;
}

bool AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
         IsServiceConnected() const {
  return is_service_connected_;
}

void AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    OnInvalidatorStateChange(syncer::InvalidatorState state) {
  if (!is_observer_ready_)
    return;

  const bool is_service_connected = (state == syncer::INVALIDATIONS_ENABLED);
  if (is_service_connected == is_service_connected_)
    return;

  is_service_connected_ = is_service_connected;
  if (is_service_connected_)
    parent_->OnInvalidationServiceConnected(invalidation_service_);
  else
    parent_->OnInvalidationServiceDisconnected(invalidation_service_);
}

void AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    OnIncomingInvalidation(
        const syncer::ObjectIdInvalidationMap& invalidation_map) {
}

std::string AffiliatedInvalidationServiceProvider::InvalidationServiceObserver::
    GetOwnerName() const {
  return "AffiliatedInvalidationService";
}

AffiliatedInvalidationServiceProvider::AffiliatedInvalidationServiceProvider()
    : invalidation_service_(nullptr),
      consumer_count_(0),
      is_shut_down_(false) {
  // The AffiliatedInvalidationServiceProvider should be created before any user
  // Profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  // Subscribe to notification about new user profiles becoming available.
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
}

AffiliatedInvalidationServiceProvider::
~AffiliatedInvalidationServiceProvider() {
  // Verify that the provider was shut down first.
  DCHECK(is_shut_down_);
}

void AffiliatedInvalidationServiceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED, type);
  DCHECK(!is_shut_down_);
  Profile* profile = content::Details<Profile>(details).ptr();
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile);
  if (!invalidation_provider) {
    // If the Profile does not support invalidation (e.g. guest, incognito),
    // ignore it.
    return;
  }
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user ||
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetUserAffiliation(user->email()) != USER_AFFILIATION_MANAGED) {
    // If the Profile belongs to a user who is not affiliated with the device's
    // enrollment domain, ignore it.
    return;
  }

  // Create a state observer for the user's invalidation service.
  invalidation::InvalidationService* invalidation_service =
      invalidation_provider->GetInvalidationService();
  profile_invalidation_service_observers_.push_back(
      new InvalidationServiceObserver(this, invalidation_service));

  if (profile_invalidation_service_observers_.back()->IsServiceConnected()) {
    // If the invalidation service is connected, check whether to switch to it.
    OnInvalidationServiceConnected(invalidation_service);
  }
}

void AffiliatedInvalidationServiceProvider::RegisterConsumer(
    Consumer* consumer) {
  if (consumers_.HasObserver(consumer) || is_shut_down_)
    return;

  consumers_.AddObserver(consumer);
  ++consumer_count_;

  if (invalidation_service_)
    consumer->OnInvalidationServiceSet(invalidation_service_);
  else if (consumer_count_ == 1)
    FindConnectedInvalidationService();
}

void AffiliatedInvalidationServiceProvider::UnregisterConsumer(
    Consumer* consumer) {
  if (!consumers_.HasObserver(consumer))
    return;

  consumers_.RemoveObserver(consumer);
  --consumer_count_;

  if (invalidation_service_ && consumer_count_ == 0) {
    invalidation_service_ = nullptr;
    DestroyDeviceInvalidationService();
  }
}

void AffiliatedInvalidationServiceProvider::Shutdown() {
  is_shut_down_ = true;

  registrar_.RemoveAll();
  profile_invalidation_service_observers_.clear();
  DestroyDeviceInvalidationService();

  if (invalidation_service_) {
    invalidation_service_ = nullptr;
    // Explicitly notify consumers that the invalidation service they were using
    // is no longer available.
    SetInvalidationService(nullptr);
  }
}

invalidation::TiclInvalidationService*
AffiliatedInvalidationServiceProvider::
    GetDeviceInvalidationServiceForTest() const {
  return device_invalidation_service_.get();
}

void AffiliatedInvalidationServiceProvider::OnInvalidationServiceConnected(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!is_shut_down_);

  if (consumer_count_ == 0) {
    // If there are no consumers, no invalidation service is required.
    return;
  }

  if (!device_invalidation_service_) {
    // The lack of a device-global invalidation service implies that another
    // connected invalidation service is being made available to consumers
    // already. There is no need to switch from that to the service which just
    // connected.
    return;
  }

  if (invalidation_service != device_invalidation_service_.get()) {
    // If an invalidation service other than the device-global one connected,
    // destroy the device-global service.
    invalidation_service_ = nullptr;
    DestroyDeviceInvalidationService();
  }

  // Make the invalidation service that just connected available to consumers.
  SetInvalidationService(invalidation_service);
}

void AffiliatedInvalidationServiceProvider::OnInvalidationServiceDisconnected(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!is_shut_down_);

  if (invalidation_service != invalidation_service_) {
    // If the invalidation service which disconnected was not being made
    // available to consumers, return.
    return;
  }

  // The invalidation service which disconnected was being made available to
  // consumers. Stop making it available.
  DCHECK(consumer_count_);
  invalidation_service_ = nullptr;

  // Try to make another invalidation service available to consumers.
  FindConnectedInvalidationService();

  // If no other connected invalidation service was found, explicitly notify
  // consumers that the invalidation service they were using is no longer
  // available.
  if (!invalidation_service_)
    SetInvalidationService(nullptr);
}

void AffiliatedInvalidationServiceProvider::FindConnectedInvalidationService() {
  DCHECK(!invalidation_service_);
  DCHECK(consumer_count_);
  DCHECK(!is_shut_down_);

  for (ScopedVector<InvalidationServiceObserver>::const_iterator it =
           profile_invalidation_service_observers_.begin();
           it != profile_invalidation_service_observers_.end(); ++it) {
    if ((*it)->IsServiceConnected()) {
      // If a connected invalidation service belonging to an affiliated
      // logged-in user is found, make it available to consumers.
      DestroyDeviceInvalidationService();
      SetInvalidationService((*it)->GetInvalidationService());
      return;
    }
  }

  if (!device_invalidation_service_) {
    // If no other connected invalidation service was found and no device-global
    // invalidation service exists, create one.
    device_invalidation_service_.reset(
        new invalidation::TiclInvalidationService(
            GetUserAgent(),
            scoped_ptr<IdentityProvider>(new chromeos::DeviceIdentityProvider(
                chromeos::DeviceOAuth2TokenServiceFactory::Get())),
            scoped_ptr<invalidation::TiclSettingsProvider>(
                new TiclDeviceSettingsProvider),
            g_browser_process->gcm_driver(),
            g_browser_process->system_request_context()));
    device_invalidation_service_->Init(
        scoped_ptr<syncer::InvalidationStateTracker>(
            new invalidation::InvalidatorStorage(
                    g_browser_process->local_state())));
    device_invalidation_service_observer_.reset(
        new InvalidationServiceObserver(
                this,
                device_invalidation_service_.get()));
  }

  if (device_invalidation_service_observer_->IsServiceConnected()) {
    // If the device-global invalidation service is connected already, make it
    // available to consumers immediately. Otherwise, the invalidation service
    // will be made available to clients when it successfully connects.
    OnInvalidationServiceConnected(device_invalidation_service_.get());
  }
}

void AffiliatedInvalidationServiceProvider::SetInvalidationService(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!invalidation_service_);
  invalidation_service_ = invalidation_service;
  FOR_EACH_OBSERVER(Consumer,
                    consumers_,
                    OnInvalidationServiceSet(invalidation_service_));
}

void AffiliatedInvalidationServiceProvider::DestroyDeviceInvalidationService() {
  device_invalidation_service_observer_.reset();
  device_invalidation_service_.reset();
}

}  // namespace policy
