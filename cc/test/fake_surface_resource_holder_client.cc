// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_surface_resource_holder_client.h"

namespace cc {

FakeSurfaceResourceHolderClient::FakeSurfaceResourceHolderClient() = default;

FakeSurfaceResourceHolderClient::~FakeSurfaceResourceHolderClient() = default;

void FakeSurfaceResourceHolderClient::ReturnResources(
    const ReturnedResourceArray& resources) {
  returned_resources_.insert(returned_resources_.end(), resources.begin(),
                             resources.end());
}

}  // namespace cc
