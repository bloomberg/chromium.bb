// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
#define CC_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include "cc/surfaces/surface_deadline_observer.h"

namespace cc {

class SurfaceDependencyDeadline : public viz::BeginFrameObserver {
 public:
  explicit SurfaceDependencyDeadline(viz::BeginFrameSource* begin_frame_source);
  ~SurfaceDependencyDeadline() override;

  void Set(uint32_t number_of_frames_to_deadline);
  void Cancel();

  void AddObserver(SurfaceDeadlineObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(SurfaceDeadlineObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  bool has_deadline() const {
    return number_of_frames_to_deadline_.has_value();
  }

  // Takes on the same viz::BeginFrameSource and deadline as |other|. Returns
  // false if they're already the same, and true otherwise.
  bool InheritFrom(const SurfaceDependencyDeadline& other);

  bool operator==(const SurfaceDependencyDeadline& other);
  bool operator!=(const SurfaceDependencyDeadline& other) {
    return !(*this == other);
  }

  // viz::BeginFrameObserver implementation.
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

 private:
  base::ObserverList<SurfaceDeadlineObserver> observer_list_;
  viz::BeginFrameSource* begin_frame_source_ = nullptr;
  base::Optional<uint32_t> number_of_frames_to_deadline_;

  viz::BeginFrameArgs last_begin_frame_args_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyDeadline);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
