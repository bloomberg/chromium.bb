// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_WIN_H_
#define NET_BASE_SSL_CONFIG_SERVICE_WIN_H_
#pragma once

#include "base/time.h"
#include "net/base/ssl_config_service.h"

namespace net {

// This class is responsible for getting and setting the SSL configuration on
// Windows.
//
// We think the SSL configuration settings should apply to all applications
// used by the user.  We consider IE's Internet Options as the de facto
// system-wide network configuration settings, so we just use the values
// from IE's Internet Settings registry key.
class SSLConfigServiceWin : public SSLConfigService {
 public:
  SSLConfigServiceWin();
  explicit SSLConfigServiceWin(base::TimeTicks now);  // Used for testing.

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
  void GetSSLConfig(SSLConfig* config) {
    GetSSLConfigAt(config, base::TimeTicks::Now());
  }

  // Used for testing.
  void GetSSLConfigAt(SSLConfig* config, base::TimeTicks now);

 private:
  virtual ~SSLConfigServiceWin() {}

  static void SetSSLVersionEnabled(int version, bool enabled);

  void UpdateConfig(base::TimeTicks now);

  // We store the IE SSL config and the time that we fetched it.
  SSLConfig config_info_;
  base::TimeTicks config_time_;
  bool ever_updated_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceWin);
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_WIN_H_
