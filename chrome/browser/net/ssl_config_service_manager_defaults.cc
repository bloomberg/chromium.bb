// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"
#include "net/base/ssl_config_service_defaults.h"

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerDefaults

// The factory for creating an SSLConfigServiceDefaults instance.
class SSLConfigServiceManagerDefaults
    : public SSLConfigServiceManager {
 public:
  SSLConfigServiceManagerDefaults()
      : ssl_config_service_(new net::SSLConfigServiceDefaults()) {
  }
  virtual ~SSLConfigServiceManagerDefaults() {}

  virtual net::SSLConfigService* Get() {
    return ssl_config_service_;
  }

 private:
  scoped_refptr<net::SSLConfigServiceDefaults> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerDefaults);
};

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    Profile* profile) {
  return new SSLConfigServiceManagerDefaults();
}
