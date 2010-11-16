// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
#pragma once

#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"

namespace chromeos {

class ProxyCrosSettingsProvider : public CrosSettingsProvider {
 public:
  ProxyCrosSettingsProvider();
  // CrosSettingsProvider implementation.
  virtual bool Get(const std::string& path, Value** out_value) const;
  virtual bool HandlesSetting(const std::string& path);

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path, Value* value);

  chromeos::ProxyConfigServiceImpl* GetConfigService() const;

  void AppendPortIfValid(const char* port_cache_key, std::string* server_uri);

  void FormServerUriIfValid(const char* host_cache_key,
      const std::string& port_num, std::string* server_uri);

  Value* CreateServerHostValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  Value* CreateServerPortValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  void SetCache(const std::string& key, const Value* value);

  // A cache to keep whatever user typed.
  DictionaryValue cache_;

  DISALLOW_COPY_AND_ASSIGN(ProxyCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
