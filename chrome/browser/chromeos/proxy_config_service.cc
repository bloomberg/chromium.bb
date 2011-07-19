// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service.h"

namespace chromeos {

ProxyConfigService::ProxyConfigService(
    const scoped_refptr<ProxyConfigServiceImpl>& impl)
    : impl_(impl) {
}

ProxyConfigService::~ProxyConfigService() {}

void ProxyConfigService::AddObserver(Observer* observer) {
  impl_->AddObserver(observer);
}

void ProxyConfigService::RemoveObserver(Observer* observer) {
  impl_->RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability ProxyConfigService::GetLatestProxyConfig(
    net::ProxyConfig* config) {
  return impl_->IOGetProxyConfig(config);
}

}  // namespace chromeos
