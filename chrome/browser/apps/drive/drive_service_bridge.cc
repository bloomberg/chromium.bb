// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_service_bridge.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/drive/drive_app_registry.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "components/drive/service/drive_api_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

// Hosts DriveAPIService and DriveAppRegistry.
// TODO(xiyuan): Optimize to leverage chromeos::DriveIntegrationService.
class DriveServiceBridgeImpl : public DriveServiceBridge,
                               public drive::DriveServiceObserver,
                               public drive::DriveNotificationObserver {
 public:
  explicit DriveServiceBridgeImpl(Profile* profile);
  ~DriveServiceBridgeImpl() override;

  void Initialize();

  // DriveServiceBridge:
  drive::DriveAppRegistry* GetAppRegistry() override;

  // drive::DriveServiceObserver:
  void OnReadyToSendRequests() override;

  // drive::DriveNotificationObserver:
  void OnNotificationReceived() override;
  void OnPushNotificationEnabled(bool enabled) override;

 private:
  Profile* profile_;
  std::unique_ptr<drive::DriveServiceInterface> drive_service_;
  std::unique_ptr<drive::DriveAppRegistry> drive_app_registry_;

  DISALLOW_COPY_AND_ASSIGN(DriveServiceBridgeImpl);
};

DriveServiceBridgeImpl::DriveServiceBridgeImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

DriveServiceBridgeImpl::~DriveServiceBridgeImpl() {
  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);

  drive_service_->RemoveObserver(this);

  drive_app_registry_.reset();
  drive_service_.reset();
}

void DriveServiceBridgeImpl::Initialize() {
  scoped_refptr<base::SequencedTaskRunner> drive_task_runner(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  drive_service_.reset(new drive::DriveAPIService(
      token_service, profile_->GetRequestContext(), drive_task_runner.get(),
      GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
      GURL(google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction),
      std::string(), /* custom_user_agent */
      NO_TRAFFIC_ANNOTATION_YET));
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  drive_service_->Initialize(signin_manager->GetAuthenticatedAccountId());
  drive_service_->AddObserver(this);

  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::GetForBrowserContext(profile_);
  if (drive_notification_manager)
    drive_notification_manager->AddObserver(this);

  drive_app_registry_.reset(new drive::DriveAppRegistry(drive_service_.get()));
  if (drive_service_->CanSendRequest())
    drive_app_registry_->Update();
}

drive::DriveAppRegistry* DriveServiceBridgeImpl::GetAppRegistry() {
  return drive_app_registry_.get();
}

void DriveServiceBridgeImpl::OnReadyToSendRequests() {
  drive_app_registry_->Update();
}

void DriveServiceBridgeImpl::OnNotificationReceived() {
  if (drive_service_->CanSendRequest())
    drive_app_registry_->Update();
}

void DriveServiceBridgeImpl::OnPushNotificationEnabled(bool enabled) {
  if (enabled && drive_service_->CanSendRequest())
    drive_app_registry_->Update();
}

}  // namespace

// static
std::unique_ptr<DriveServiceBridge> DriveServiceBridge::Create(
    Profile* profile) {
  std::unique_ptr<DriveServiceBridgeImpl> bridge(
      new DriveServiceBridgeImpl(profile));
  bridge->Initialize();
  return std::move(bridge);
}

// static
void DriveServiceBridge::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  DCHECK(factories);
  factories->insert(ProfileOAuth2TokenServiceFactory::GetInstance());
  factories->insert(SigninManagerFactory::GetInstance());
  factories->insert(drive::DriveNotificationManagerFactory::GetInstance());
}
