// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/network_quality_provider_stub.h"

namespace offline_pages {

const char kOfflineNQPKey[] = "OfflineNQP";

NetworkQualityProviderStub::NetworkQualityProviderStub()
    : connection_type_(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G) {}

NetworkQualityProviderStub::~NetworkQualityProviderStub() {}

// static
NetworkQualityProviderStub* NetworkQualityProviderStub::GetUserData(
    base::SupportsUserData* supports_user_data) {
  return static_cast<NetworkQualityProviderStub*>(
      supports_user_data->GetUserData(&kOfflineNQPKey));
}

// static
void NetworkQualityProviderStub::SetUserData(
    base::SupportsUserData* supports_user_data,
    NetworkQualityProviderStub* stub) {
  DCHECK(supports_user_data);
  DCHECK(stub);
  supports_user_data->SetUserData(&kOfflineNQPKey, stub);
}

void NetworkQualityProviderStub::AddEffectiveConnectionTypeObserver(
    net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer) {}

void NetworkQualityProviderStub::RemoveEffectiveConnectionTypeObserver(
    net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer) {}

net::EffectiveConnectionType
NetworkQualityProviderStub::GetEffectiveConnectionType() const {
  return connection_type_;
}
}  // namespace offline_pages
