// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/unified_consent_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/unified_consent_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/unified_consent_service_client.h"

namespace {

using Service = unified_consent::UnifiedConsentServiceClient::Service;
using ServiceState = unified_consent::UnifiedConsentServiceClient::ServiceState;

class FakeUnifiedConsentServiceClient
    : public unified_consent::UnifiedConsentServiceClient {
 public:
  FakeUnifiedConsentServiceClient() = default;
  ~FakeUnifiedConsentServiceClient() override = default;

  // UnifiedConsentServiceClient:
  ServiceState GetServiceState(Service service) override {
    return ServiceState::kNotSupported;
  }
  void SetServiceEnabled(Service service, bool enabled) override {}
};

}  // namespace

std::unique_ptr<KeyedService> BuildUnifiedConsentServiceForTesting(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!IsUnifiedConsentEnabled(profile))
    return nullptr;

  return std::make_unique<unified_consent::UnifiedConsentService>(
      std::make_unique<FakeUnifiedConsentServiceClient>(), profile->GetPrefs(),
      IdentityManagerFactory::GetForProfile(profile),
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile));
}
