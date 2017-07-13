// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_surface_observer.h"

namespace cc {

FakeSurfaceObserver::FakeSurfaceObserver(bool damage_display)
    : damage_display_(damage_display) {}

FakeSurfaceObserver::~FakeSurfaceObserver() {}

void FakeSurfaceObserver::Reset() {
  last_ack_ = BeginFrameAck();
  damaged_surfaces_.clear();
  will_draw_surfaces_.clear();
  last_surface_info_ = viz::SurfaceInfo();
  last_created_surface_id_ = viz::SurfaceId();
}

bool FakeSurfaceObserver::IsSurfaceDamaged(
    const viz::SurfaceId& surface_id) const {
  return damaged_surfaces_.count(surface_id) > 0;
}

bool FakeSurfaceObserver::SurfaceWillDrawCalled(
    const viz::SurfaceId& surface_id) const {
  return will_draw_surfaces_.count(surface_id) > 0;
}

bool FakeSurfaceObserver::OnSurfaceDamaged(const viz::SurfaceId& surface_id,
                                           const BeginFrameAck& ack) {
  if (ack.has_damage)
    damaged_surfaces_.insert(surface_id);
  last_ack_ = ack;
  return ack.has_damage && damage_display_;
}

void FakeSurfaceObserver::OnSurfaceWillDraw(const viz::SurfaceId& surface_id) {
  will_draw_surfaces_.insert(surface_id);
}

void FakeSurfaceObserver::OnSurfaceCreated(
    const viz::SurfaceInfo& surface_info) {
  last_created_surface_id_ = surface_info.id();
  last_surface_info_ = surface_info;
}

}  // namespace cc
