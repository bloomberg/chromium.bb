// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_invalidator.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/ticl_device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_identity_provider.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"

class Profile;

namespace policy {

class DeviceCloudPolicyInvalidator::InvalidationServiceObserver
    : public syncer::InvalidationHandler {
 public:
  explicit InvalidationServiceObserver(
      DeviceCloudPolicyInvalidator* parent,
      invalidation::InvalidationService* invalidation_service);
  virtual ~InvalidationServiceObserver();

  invalidation::InvalidationService* GetInvalidationService();
  bool IsServiceConnected() const;

  // public syncer::InvalidationHandler:
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual std::string GetOwnerName() const OVERRIDE;

 private:
  DeviceCloudPolicyInvalidator* parent_;
  invalidation::InvalidationService* invalidation_service_;
  bool is_service_connected_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceObserver);
};

DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    InvalidationServiceObserver(
        DeviceCloudPolicyInvalidator* parent,
        invalidation::InvalidationService* invalidation_service)
    : parent_(parent),
      invalidation_service_(invalidation_service),
      is_service_connected_(invalidation_service->GetInvalidatorState() ==
                                syncer::INVALIDATIONS_ENABLED) {
  invalidation_service_->RegisterInvalidationHandler(this);
}

DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    ~InvalidationServiceObserver() {
  invalidation_service_->UnregisterInvalidationHandler(this);
}

invalidation::InvalidationService*
DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    GetInvalidationService() {
  return invalidation_service_;
}

bool DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
         IsServiceConnected() const {
  return is_service_connected_;
}

void DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    OnInvalidatorStateChange(syncer::InvalidatorState state) {
  const bool is_service_connected = (state == syncer::INVALIDATIONS_ENABLED);
  if (is_service_connected == is_service_connected_)
    return;

  is_service_connected_ = is_service_connected;
  if (is_service_connected_)
    parent_->OnInvalidationServiceConnected(invalidation_service_);
  else
    parent_->OnInvalidationServiceDisconnected(invalidation_service_);
}

void DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    OnIncomingInvalidation(
        const syncer::ObjectIdInvalidationMap& invalidation_map) {
}

std::string DeviceCloudPolicyInvalidator::InvalidationServiceObserver::
    GetOwnerName() const {
  return "DevicePolicy";
}

DeviceCloudPolicyInvalidator::DeviceCloudPolicyInvalidator()
    : invalidation_service_(NULL),
      highest_handled_invalidation_version_(0) {
  // The DeviceCloudPolicyInvalidator should be created before any user
  // Profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  // Subscribe to notification about new user profiles becoming available.
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());

  TryToCreateInvalidator();
}

DeviceCloudPolicyInvalidator::~DeviceCloudPolicyInvalidator() {
  DestroyInvalidator();
}

void DeviceCloudPolicyInvalidator::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED, type);
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          content::Details<Profile>(details).ptr());
  if (!invalidation_provider) {
    // If the Profile does not support invalidation (e.g. guest, incognito),
    // ignore it.
    return;
  }

  // Create a state observer for the user's invalidation service.
  profile_invalidation_service_observers_.push_back(
      new InvalidationServiceObserver(
              this,
              invalidation_provider->GetInvalidationService()));

  TryToCreateInvalidator();
}

void DeviceCloudPolicyInvalidator::OnInvalidationServiceConnected(
    invalidation::InvalidationService* invalidation_service) {
  if (!device_invalidation_service_) {
    // The lack of a device-global invalidation service implies that a
    // |CloudPolicyInvalidator| backed by another connected service exists
    // already. There is no need to switch from that to the service which just
    // connected.
    return;
  }

  if (invalidation_service != device_invalidation_service_.get()) {
    // If an invalidation service other than the device-global one connected,
    // destroy the device-global service and the |CloudPolicyInvalidator| backed
    // by it, if any.
    DestroyInvalidator();
    DestroyDeviceInvalidationService();
  }

  // Create a |CloudPolicyInvalidator| backed by the invalidation service which
  // just connected.
  CreateInvalidator(invalidation_service);
}

void DeviceCloudPolicyInvalidator::OnInvalidationServiceDisconnected(
    invalidation::InvalidationService* invalidation_service) {
  if (invalidation_service != invalidation_service_) {
    // If the invalidation service which disconnected is not backing the current
    // |CloudPolicyInvalidator|, return.
    return;
  }

  // Destroy the |CloudPolicyInvalidator| backed by the invalidation service
  // which just disconnected.
  DestroyInvalidator();

  // Try to create a |CloudPolicyInvalidator| backed by another invalidation
  // service.
  TryToCreateInvalidator();
}

void DeviceCloudPolicyInvalidator::TryToCreateInvalidator() {
  if (invalidator_) {
    // If a |CloudPolicyInvalidator| exists already, return.
    return;
  }

  for (ScopedVector<InvalidationServiceObserver>::const_iterator it =
           profile_invalidation_service_observers_.begin();
           it != profile_invalidation_service_observers_.end(); ++it) {
    if ((*it)->IsServiceConnected()) {
      // If a connected invalidation service belonging to a logged-in user is
      // found, create a |CloudPolicyInvalidator| backed by that service and
      // destroy the device-global service, if any.
      DestroyDeviceInvalidationService();
      CreateInvalidator((*it)->GetInvalidationService());
      return;
    }
  }

  if (!device_invalidation_service_) {
    // If no other connected invalidation service was found, ensure that a
    // device-global service is running.
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
    // If the device-global invalidation service is connected, create a
    // |CloudPolicyInvalidator| backed by it. Otherwise,  a
    // |CloudPolicyInvalidator| will be created later when a connected service
    // becomes available.
    CreateInvalidator(device_invalidation_service_.get());
  }
}

void DeviceCloudPolicyInvalidator::CreateInvalidator(
    invalidation::InvalidationService* invalidation_service) {
  invalidation_service_ = invalidation_service;
  DCHECK(!invalidator_);
  invalidator_.reset(new CloudPolicyInvalidator(
      enterprise_management::DeviceRegisterRequest::DEVICE,
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetDeviceCloudPolicyManager()->core(),
      base::MessageLoopProxy::current(),
      scoped_ptr<base::Clock>(new base::DefaultClock()),
      highest_handled_invalidation_version_));
  invalidator_->Initialize(invalidation_service);
}

void DeviceCloudPolicyInvalidator::DestroyInvalidator() {
  if (invalidator_) {
    highest_handled_invalidation_version_ =
        invalidator_->highest_handled_invalidation_version();
    invalidator_->Shutdown();
  }
  invalidator_.reset();
  invalidation_service_ = NULL;
}

void DeviceCloudPolicyInvalidator::DestroyDeviceInvalidationService() {
  device_invalidation_service_observer_.reset();
  device_invalidation_service_.reset();
}

}  // namespace policy
