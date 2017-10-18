// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_

namespace viz {

class SurfaceId;
class SurfaceInfo;
struct BeginFrameAck;
struct BeginFrameArgs;

class SurfaceObserver {
 public:
  // Called when a CompositorFrame with a new SurfaceId activates for the first
  // time.
  virtual void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) = 0;

  // Called when a CompositorFrame within |surface| activates.
  virtual void OnSurfaceActivated(const SurfaceId& surface_id) = 0;

  // Called when a Surface was marked to be destroyed.
  virtual void OnSurfaceDestroyed(const SurfaceId& surface_id) = 0;

  // Called when a Surface is modified, e.g. when a CompositorFrame is
  // activated, its producer confirms that no CompositorFrame will be submitted
  // in response to a BeginFrame, or a CopyOutputRequest is issued.
  //
  // |ack.sequence_number| is only valid if called in response to a BeginFrame.
  // Should return true if this causes a Display to be damaged.
  virtual bool OnSurfaceDamaged(const SurfaceId& surface_id,
                                const BeginFrameAck& ack) = 0;

  // Called when a surface is garbage-collected.
  virtual void OnSurfaceDiscarded(const SurfaceId& surface_id) = 0;

  // Called when a Surface's CompositorFrame producer has received a BeginFrame
  // and, thus, is expected to produce damage soon.
  virtual void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                       const BeginFrameArgs& args) = 0;

  // Called when |surface_id| or one of its descendents is determined to be
  // damaged at aggregation time.
  // TODO(crbug.com/776098): This is only used in tests. We can probably remove
  // it.
  virtual void OnSurfaceSubtreeDamaged(const SurfaceId& surface_id) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_
