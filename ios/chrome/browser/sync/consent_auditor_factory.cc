// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/850428): Move this and .h back to
// ios/chrome/browser/consent_auditor, when it does not depend on
// UserEventService anymore. Currently this is not possible due to a BUILD.gn
// depedency.

#include "ios/chrome/browser/sync/consent_auditor_factory.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/consent_auditor/consent_auditor_impl.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/consent_auditor/consent_sync_bridge_impl.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"

// static
consent_auditor::ConsentAuditor* ConsentAuditorFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
consent_auditor::ConsentAuditor*
ConsentAuditorFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  return base::Singleton<ConsentAuditorFactory>::get();
}

ConsentAuditorFactory::ConsentAuditorFactory()
    : BrowserStateKeyedServiceFactory(
          "ConsentAuditor",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
}

ConsentAuditorFactory::~ConsentAuditorFactory() {}

std::unique_ptr<KeyedService> ConsentAuditorFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ios::ChromeBrowserState* ios_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);

  std::unique_ptr<syncer::ConsentSyncBridge> consent_sync_bridge;
  syncer::UserEventService* user_event_service = nullptr;
  if (base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType)) {
    syncer::OnceModelTypeStoreFactory store_factory =
        ModelTypeStoreServiceFactory::GetForBrowserState(ios_browser_state)
            ->GetStoreFactory();
    auto change_processor =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::USER_CONSENTS,
            base::BindRepeating(&syncer::ReportUnrecoverableError,
                                ::GetChannel()));
    consent_sync_bridge = std::make_unique<syncer::ConsentSyncBridgeImpl>(
        std::move(store_factory), std::move(change_processor));
  } else {
    user_event_service =
        IOSUserEventServiceFactory::GetForBrowserState(ios_browser_state);
  }

  return std::make_unique<consent_auditor::ConsentAuditorImpl>(
      ios_browser_state->GetPrefs(), std::move(consent_sync_bridge),
      user_event_service,
      // The browser version and locale do not change runtime, so we can pass
      // them directly.
      version_info::GetVersionNumber(),
      GetApplicationContext()->GetApplicationLocale());
}

bool ConsentAuditorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

void ConsentAuditorFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  consent_auditor::ConsentAuditorImpl::RegisterProfilePrefs(registry);
}
