// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_H_
#pragma once

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

namespace chromeos {

// Wrapper class for the RefCountedThreadSafe chromeos::ProxyConfigServiceImpl
// that forwards net::ProxyConfigService interface to the actual implementation,
// instantiated in ProfileIOData::CreateProxyConfigService.
class ProxyConfigService : public net::ProxyConfigService {
 public:
  explicit ProxyConfigService(
      const scoped_refptr<ProxyConfigServiceImpl>& impl);
  virtual ~ProxyConfigService();

  // ProxyConfigService methods.  Called from IO thread.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual ConfigAvailability GetLatestProxyConfig(net::ProxyConfig* config);

 private:
  scoped_refptr<ProxyConfigServiceImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_H_
