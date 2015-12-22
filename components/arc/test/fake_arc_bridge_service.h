// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

class FakeArcBridgeService : public ArcBridgeService {
 public:
  FakeArcBridgeService();
  ~FakeArcBridgeService() override;

  // arc::ArcBridgeService
  void DetectAvailability() override;
  void HandleStartup() override;
  void Shutdown() override;

  void SetReady();

  void SetStopped();

  bool HasObserver(const Observer* observer);

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_
