// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using send_tab_to_self::SendTabToSelfSyncService;

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  static base::NoDestructor<SendTabToSelfSyncServiceFactory> instance;
  return instance.get();
}

// static
SendTabToSelfSyncService* SendTabToSelfSyncServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SendTabToSelfSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() {}

std::unique_ptr<KeyedService>
SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetLocalDeviceInfoProvider();

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory();

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  return std::make_unique<SendTabToSelfSyncService>(
      GetChannel(), local_device_info_provider, std::move(store_factory),
      history_service);
}
