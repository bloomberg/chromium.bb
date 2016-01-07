// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_AUTH_ARC_AUTH_SERVICE_H_
#define COMPONENTS_ARC_AUTH_ARC_AUTH_SERVICE_H_

namespace arc {

class ArcAuthService {
 public:
  virtual ~ArcAuthService() {}

  // Starts listening to state changes of the ArcBridgeService.
  virtual void StartObservingBridgeServiceChanges() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_AUTH_ARC_AUTH_SERVICE_H_
