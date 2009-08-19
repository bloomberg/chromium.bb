// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"
#include "net/base/ssl_config_service_win.h"

class Profile;

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerWin

// The factory for creating an SSLConfigServiceWin instance.
class SSLConfigServiceManagerWin
    : public SSLConfigServiceManager {
 public:
  SSLConfigServiceManagerWin()
      : ssl_config_service_(new net::SSLConfigServiceWin) {
  }
  virtual ~SSLConfigServiceManagerWin() {}

  virtual net::SSLConfigService* Get() {
    return ssl_config_service_;
  }

 private:
  scoped_refptr<net::SSLConfigService> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerWin);
};

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    Profile* profile) {
  return new SSLConfigServiceManagerWin();
}
