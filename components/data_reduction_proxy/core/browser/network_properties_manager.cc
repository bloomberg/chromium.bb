// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"

namespace data_reduction_proxy {

NetworkPropertiesManager::NetworkPropertiesManager() {}

NetworkPropertiesManager::~NetworkPropertiesManager() {}

bool NetworkPropertiesManager::IsSecureProxyAllowed() const {
  return !network_properties_.secure_proxy_disallowed_by_carrier() &&
         !network_properties_.has_captive_portal() &&
         !network_properties_.secure_proxy_flags()
              .disallowed_due_to_warmup_probe_failure();
}

bool NetworkPropertiesManager::IsInsecureProxyAllowed() const {
  return !network_properties_.insecure_proxy_flags()
              .disallowed_due_to_warmup_probe_failure();
}

bool NetworkPropertiesManager::IsSecureProxyDisallowedByCarrier() const {
  return network_properties_.secure_proxy_disallowed_by_carrier();
}

void NetworkPropertiesManager::SetIsSecureProxyDisallowedByCarrier(
    bool disallowed_by_carrier) {
  network_properties_.set_secure_proxy_disallowed_by_carrier(
      disallowed_by_carrier);
}

bool NetworkPropertiesManager::IsCaptivePortal() const {
  return network_properties_.has_captive_portal();
}

void NetworkPropertiesManager::SetIsCaptivePortal(bool is_captive_portal) {
  network_properties_.set_has_captive_portal(is_captive_portal);
}

bool NetworkPropertiesManager::HasWarmupURLProbeFailed(
    bool secure_proxy) const {
  return secure_proxy ? network_properties_.secure_proxy_flags()
                            .disallowed_due_to_warmup_probe_failure()
                      : network_properties_.insecure_proxy_flags()
                            .disallowed_due_to_warmup_probe_failure();
}

void NetworkPropertiesManager::SetHasWarmupURLProbeFailed(
    bool secure_proxy,
    bool warmup_url_probe_failed) {
  if (secure_proxy) {
    network_properties_.mutable_secure_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  } else {
    network_properties_.mutable_insecure_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  }
}

}  // namespace data_reduction_proxy