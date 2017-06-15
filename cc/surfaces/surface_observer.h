// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_OBSERVER_H_
#define CC_SURFACES_SURFACE_OBSERVER_H_

namespace cc {

struct BeginFrameAck;
struct BeginFrameArgs;
class SurfaceId;
class SurfaceInfo;

class SurfaceObserver {
 public:
  // Runs when a CompositorFrame is activated for the given SurfaceInfo for the
  // first time.
  virtual void OnSurfaceCreated(const SurfaceInfo& surface_info) = 0;

  // Runs when a Surface was marked to be destroyed.
  virtual void OnSurfaceDestroyed(const SurfaceId& surface_id) = 0;

  // Runs when a Surface is modified, e.g. when a CompositorFrame is
  // activated, its producer confirms that no CompositorFrame will be submitted
  // in response to a BeginFrame, or a CopyOutputRequest is issued.
  //
  // |ack.sequence_number| is only valid if called in response to a BeginFrame.
  // Should return true if this causes a Display to be damaged.
  virtual bool OnSurfaceDamaged(const SurfaceId& surface_id,
                                const BeginFrameAck& ack) = 0;

  // Called when a surface is garbage-collected.
  virtual void OnSurfaceDiscarded(const SurfaceId& surface_id) = 0;

  // Runs when a Surface's CompositorFrame producer has received a BeginFrame
  // and, thus, is expected to produce damage soon.
  virtual void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                       const BeginFrameArgs& args) = 0;

  // Runs when a surface has been added to the aggregated CompositorFrame.
  virtual void OnSurfaceWillDraw(const SurfaceId& surface_id) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_OBSERVER_H_
