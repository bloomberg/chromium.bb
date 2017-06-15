// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SURFACE_OBSERVER_H_
#define CC_TEST_FAKE_SURFACE_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_observer.h"

namespace cc {

class FakeSurfaceObserver : public SurfaceObserver {
 public:
  // If |damage_display| is true, the observer will indicate display damage when
  // a surface is damaged.
  explicit FakeSurfaceObserver(bool damage_display = true);
  virtual ~FakeSurfaceObserver();

  const BeginFrameAck& last_ack() const { return last_ack_; }

  bool IsSurfaceDamaged(const SurfaceId& surface_id) const;

  bool SurfaceWillDrawCalled(const SurfaceId& surface_id) const;

  const SurfaceId& last_created_surface_id() const {
    return last_created_surface_id_;
  }

  const SurfaceInfo& last_surface_info() const { return last_surface_info_; }

  void Reset();

 private:
  // SurfaceObserver implementation:
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override {}
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override {}
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override {}
  void OnSurfaceWillDraw(const SurfaceId& surface_id) override;

  bool damage_display_;
  BeginFrameAck last_ack_;
  base::flat_set<SurfaceId> damaged_surfaces_;
  base::flat_set<SurfaceId> will_draw_surfaces_;
  SurfaceId last_created_surface_id_;
  SurfaceInfo last_surface_info_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SURFACE_OBSERVER_H_
