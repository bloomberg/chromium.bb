// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/user_event_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/user_events/no_op_user_event_service.h"
#include "components/sync/user_events/user_event_service_impl.h"
#include "components/sync/user_events/user_event_sync_bridge.h"

namespace browser_sync {

// static
UserEventServiceFactory* UserEventServiceFactory::GetInstance() {
  return base::Singleton<UserEventServiceFactory>::get();
}

// static
syncer::UserEventService* UserEventServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::UserEventService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEventServiceFactory::UserEventServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserEventService",
          BrowserContextDependencyManager::GetInstance()) {}

UserEventServiceFactory::~UserEventServiceFactory() {}

KeyedService* UserEventServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!syncer::UserEventServiceImpl::MightRecordEvents(
          context->IsOffTheRecord(), sync_service)) {
    return new syncer::NoOpUserEventService();
  }

  syncer::OnceModelTypeStoreFactory store_factory =
      browser_sync::ProfileSyncService::GetModelTypeStoreFactory(
          profile->GetPath());
  syncer::ModelTypeSyncBridge::ChangeProcessorFactory processor_factory =
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::Create,
                          base::BindRepeating(&syncer::ReportUnrecoverableError,
                                              chrome::GetChannel()));
  auto bridge = std::make_unique<syncer::UserEventSyncBridge>(
      std::move(store_factory), std::move(processor_factory),
      sync_service->GetGlobalIdMapper());
  return new syncer::UserEventServiceImpl(sync_service, std::move(bridge));
}

content::BrowserContext* UserEventServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace browser_sync
