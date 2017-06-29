// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SURFACE_RESOURCE_HOLDER_CLIENT_H_
#define CC_TEST_FAKE_SURFACE_RESOURCE_HOLDER_CLIENT_H_

#include "cc/surfaces/surface_resource_holder_client.h"

namespace cc {

class FakeSurfaceResourceHolderClient : public SurfaceResourceHolderClient {
 public:
  FakeSurfaceResourceHolderClient();
  ~FakeSurfaceResourceHolderClient() override;

  // SurfaceResourceHolderClient implementation.
  void ReturnResources(const std::vector<ReturnedResource>& resources) override;

  void clear_returned_resources() { returned_resources_.clear(); }
  const std::vector<ReturnedResource>& returned_resources() {
    return returned_resources_;
  }

 private:
  std::vector<ReturnedResource> returned_resources_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SURFACE_RESOURCE_HOLDER_CLIENT_H_
