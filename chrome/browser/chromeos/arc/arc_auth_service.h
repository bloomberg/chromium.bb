// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/auth.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// This class proxies the request from the client to fetch an auth code from
// LSO.
class ArcAuthService : public ArcService,
                       public AuthHost,
                       public ArcBridgeService::Observer {
 public:
  explicit ArcAuthService(ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

 private:
  // Overrides ArcBridgeService::Observer.
  void OnAuthInstanceReady() override;

  // Overrides AuthHost.
  void GetAuthCode(const GetAuthCodeCallback& callback) override;

  mojo::Binding<AuthHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
