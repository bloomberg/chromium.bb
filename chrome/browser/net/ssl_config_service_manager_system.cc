// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"
#include "net/base/ssl_config_service.h"

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerSystem

// The manager for holding a system SSLConfigService instance.  System
// SSLConfigService objects do not depend on the profile.
class SSLConfigServiceManagerSystem
    : public SSLConfigServiceManager {
 public:
  SSLConfigServiceManagerSystem()
      : ssl_config_service_(
          net::SSLConfigService::CreateSystemSSLConfigService()) {
  }
  virtual ~SSLConfigServiceManagerSystem() {}

  virtual net::SSLConfigService* Get() {
    return ssl_config_service_;
  }

 private:
  scoped_refptr<net::SSLConfigService> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerSystem);
};

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    PrefService* user_prefs,
    PrefService* local_state) {
  return new SSLConfigServiceManagerSystem();
}
