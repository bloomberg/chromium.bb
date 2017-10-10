// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include "components/viz/service/surfaces/surface_deadline_client.h"

namespace viz {

class SurfaceDependencyDeadline : public BeginFrameObserver {
 public:
  SurfaceDependencyDeadline(SurfaceDeadlineClient* client,
                            BeginFrameSource* begin_frame_source);
  ~SurfaceDependencyDeadline() override;

  void Set(uint32_t number_of_frames_to_deadline);
  void Cancel();

  bool has_deadline() const {
    return number_of_frames_to_deadline_.has_value();
  }

  // Takes on the same BeginFrameSource and deadline as |other|. Returns
  // false if they're already the same, and true otherwise.
  bool InheritFrom(const SurfaceDependencyDeadline& other);

  bool operator==(const SurfaceDependencyDeadline& other);
  bool operator!=(const SurfaceDependencyDeadline& other) {
    return !(*this == other);
  }

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

 private:
  SurfaceDeadlineClient* const client_;
  BeginFrameSource* begin_frame_source_ = nullptr;
  base::Optional<uint32_t> number_of_frames_to_deadline_;

  BeginFrameArgs last_begin_frame_args_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyDeadline);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
