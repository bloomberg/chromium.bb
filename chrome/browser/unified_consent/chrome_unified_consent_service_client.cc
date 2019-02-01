// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"

ChromeUnifiedConsentServiceClient::ChromeUnifiedConsentServiceClient(
    PrefService* pref_service) {}

ChromeUnifiedConsentServiceClient::~ChromeUnifiedConsentServiceClient() {}

ChromeUnifiedConsentServiceClient::ServiceState
ChromeUnifiedConsentServiceClient::GetServiceState(Service service) {
  return ServiceState::kNotSupported;
}

void ChromeUnifiedConsentServiceClient::SetServiceEnabled(Service service,
                                                          bool enabled) {}
