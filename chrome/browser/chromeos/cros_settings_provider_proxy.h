// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_PROXY_H_
#pragma once

#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"

namespace chromeos {

class CrosSettingsProviderProxy : public CrosSettingsProvider {
 public:
  CrosSettingsProviderProxy();
  virtual void Set(const std::string& path, Value* in_value);
  virtual bool Get(const std::string& path, Value** out_value) const;
  virtual bool HandlesSetting(const std::string& path);

 private:
  void AppendPortIfValid(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
      std::string* server_uri);

  bool FormServerUriIfValid(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
      const std::string& port_num, std::string* server_uri);

  Value* CreateServerHostValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  Value* CreateServerPortValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  DISALLOW_COPY_AND_ASSIGN(CrosSettingsProviderProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_PROXY_H_
