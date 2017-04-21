// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CC_SURFACES_SURFACE_RESOURCE_HOLDER_CLIENT_H_
#define CC_SURFACES_SURFACE_RESOURCE_HOLDER_CLIENT_H_

#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class CC_SURFACES_EXPORT SurfaceResourceHolderClient {
 public:
  virtual ~SurfaceResourceHolderClient() = default;

  // ReturnResources gets called when the display compositor is done using the
  // resources so that the client can use them.
  virtual void ReturnResources(const ReturnedResourceArray& resources) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_RESOURCE_HOLDER_CLIENT_H_
