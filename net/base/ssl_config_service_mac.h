// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_MAC_H_
#define NET_BASE_SSL_CONFIG_SERVICE_MAC_H_
#pragma once

#include "base/time.h"
#include "net/base/ssl_config_service.h"

namespace net {

// This class is responsible for getting and setting the SSL configuration on
// Mac OS X.
class SSLConfigServiceMac : public SSLConfigService {
 public:
  SSLConfigServiceMac();
  explicit SSLConfigServiceMac(base::TimeTicks now);  // Used for testing.

  // Get the current SSL configuration settings.  Can be called on any
  // thread.
  static bool GetSSLConfigNow(SSLConfig* config);

  // Setters.  Can be called on any thread.
  static void SetRevCheckingEnabled(bool enabled);
  static void SetSSL3Enabled(bool enabled);
  static void SetTLS1Enabled(bool enabled);

  // Get the (cached) SSL configuration settings that are fresh within 10
  // seconds.  This is cheaper than GetSSLConfigNow and is suitable when
  // we don't need the absolutely current configuration settings.  This
  // method is not thread-safe, so it must be called on the same thread.
  virtual void GetSSLConfig(SSLConfig* config);

  // Used for testing.
  void GetSSLConfigAt(SSLConfig* config, base::TimeTicks now);

 private:
  virtual ~SSLConfigServiceMac();

  void UpdateConfig(base::TimeTicks now);

  // We store the system SSL config and the time that we fetched it.
  SSLConfig config_info_;
  base::TimeTicks config_time_;
  bool ever_updated_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceMac);
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_MAC_H_
