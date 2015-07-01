// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/proximity_auth_facade.h"

#include "base/lazy_instance.h"
#include "components/proximity_auth/screenlock_bridge.h"

namespace {

// A facade class that is the glue required to initialize and manage the
// lifecycle of various objects of the Proximity Auth component.
class ProximityAuthFacade {
 public:
  proximity_auth::ScreenlockBridge* GetScreenlockBridge() {
    return &screenlock_bridge_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<ProximityAuthFacade>;
  friend struct base::DefaultDeleter<ProximityAuthFacade>;

  ProximityAuthFacade() {}
  ~ProximityAuthFacade() {}

  proximity_auth::ScreenlockBridge screenlock_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthFacade);
};

base::LazyInstance<ProximityAuthFacade> g_proximity_auth_facade_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

proximity_auth::ScreenlockBridge* GetScreenlockBridgeInstance() {
  return g_proximity_auth_facade_instance.Pointer()->GetScreenlockBridge();
}
