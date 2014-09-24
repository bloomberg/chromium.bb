// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_CHROMEOS_GCM_CONNECTION_OBSERVER_H_
#define CHROME_BROWSER_SERVICES_GCM_CHROMEOS_GCM_CONNECTION_OBSERVER_H_

#include "base/compiler_specific.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "net/base/ip_endpoint.h"

namespace gcm {

class ChromeOSGCMConnectionObserver : public GCMConnectionObserver {
 public:
  ChromeOSGCMConnectionObserver();
  virtual ~ChromeOSGCMConnectionObserver();
  // gcm::GCMConnectionObserver implementation:
  virtual void OnConnected(const net::IPEndPoint& ip_endpoint) OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

  static void ErrorCallback(
      const std::string& error_name,
      const std::string& error);

 private:
  net::IPEndPoint ip_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSGCMConnectionObserver);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_CHROMEOS_GCM_CONNECTION_OBSERVER_H_
