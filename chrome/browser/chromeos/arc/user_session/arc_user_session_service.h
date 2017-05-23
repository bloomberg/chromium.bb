// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace arc {

class ArcBridgeService;

class ArcUserSessionService
    : public ArcService,
      public InstanceHolder<mojom::IntentHelperInstance>::Observer,
      public session_manager::SessionManagerObserver {
 public:
  explicit ArcUserSessionService(ArcBridgeService* bridge_service);
  ~ArcUserSessionService() override;

  // InstanceHolder<mojom::IntentHelperInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcUserSessionService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
