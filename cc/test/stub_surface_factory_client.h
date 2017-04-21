// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_SURFACE_FACTORY_CLIENT_H_
#define CC_TEST_STUB_SURFACE_FACTORY_CLIENT_H_

#include "cc/surfaces/surface_factory_client.h"

namespace cc {

class StubSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  StubSurfaceFactoryClient() = default;
  ~StubSurfaceFactoryClient() override = default;

  void ReferencedSurfacesChanged(
      const LocalSurfaceId& local_surface_id,
      const std::vector<SurfaceId>* active_referenced_surfaces) override {}
};

}  // namespace cc

#endif  //  CC_TEST_STUB_SURFACE_FACTORY_CLIENT_H_
