// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_CLIENT_H_
#define CC_SURFACES_SURFACE_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class Surface;
struct ReturnedResource;
struct TransferableResource;

class CC_SURFACES_EXPORT SurfaceClient {
 public:
  SurfaceClient() = default;

  virtual ~SurfaceClient() = default;

  // Called when |surface| has a new CompositorFrame available for display.
  virtual void OnSurfaceActivated(Surface* surface) = 0;

  // Increments the reference count on resources specified by |resources|.
  virtual void RefResources(
      const std::vector<TransferableResource>& resources) = 0;

  // Decrements the reference count on resources specified by |resources|.
  virtual void UnrefResources(
      const std::vector<ReturnedResource>& resources) = 0;

  // ReturnResources gets called when the display compositor is done using the
  // resources so that the client can use them.
  virtual void ReturnResources(
      const std::vector<ReturnedResource>& resources) = 0;

  // Increments the reference count of resources received from a child
  // compositor.
  virtual void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceClient);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_CLIENT_H_
