// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/unified_consent_service_client_impl.h"

UnifiedConsentServiceClientImpl::ServiceState
UnifiedConsentServiceClientImpl::GetServiceState(Service service) {
  switch (service) {
    case Service::kSafeBrowsingExtendedReporting:
    case Service::kSpellCheck:
    case Service::kContextualSearch:
      return ServiceState::kNotSupported;
  }
  NOTREACHED();
}

void UnifiedConsentServiceClientImpl::SetServiceEnabled(Service service,
                                                        bool enabled) {
  if (GetServiceState(service) == ServiceState::kNotSupported)
    return;

  NOTREACHED();
}
