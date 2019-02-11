// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "ui/base/device_form_factor.h"

// static
send_tab_to_self::SendTabToSelfSyncService*
SendTabToSelfSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<send_tab_to_self::SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  return base::Singleton<SendTabToSelfSyncServiceFactory>::get();
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SendTabToSelfSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() {}

KeyedService* SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetLocalDeviceInfoProvider();

  // TODO(jeffreycohen): use KeyedService to provide a DeviceInfo ptr.

  return new send_tab_to_self::SendTabToSelfSyncService(
      chrome::GetChannel(), local_device_info_provider);
}
