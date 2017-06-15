// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_EVICTION_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_EVICTION_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT FrameEvictionManagerClient {
 public:
  virtual ~FrameEvictionManagerClient() {}
  virtual void EvictCurrentFrame() = 0;
};

// This class is responsible for globally managing which renderers keep their
// compositor frame when offscreen. We actively discard compositor frames for
// offscreen tabs, but keep a minimum amount, as an LRU cache, to make switching
// between a small set of tabs faster. The limit is a soft limit, because
// clients can lock their frame to prevent it from being discarded, e.g. if the
// tab is visible, or while capturing a screenshot.
class VIZ_SERVICE_EXPORT FrameEvictionManager
    : public base::MemoryCoordinatorClient {
 public:
  static FrameEvictionManager* GetInstance();

  void AddFrame(FrameEvictionManagerClient*, bool locked);
  void RemoveFrame(FrameEvictionManagerClient*);
  void LockFrame(FrameEvictionManagerClient*);
  void UnlockFrame(FrameEvictionManagerClient*);

  size_t GetMaxNumberOfSavedFrames() const;

  // For testing only
  void set_max_number_of_saved_frames(size_t max_number_of_saved_frames) {
    max_number_of_saved_frames_ = max_number_of_saved_frames;
  }
  void set_max_handles(float max_handles) { max_handles_ = max_handles; }

  // React on memory pressure events to adjust the number of cached frames.
  // Please make this private when crbug.com/443824 has been fixed.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

 private:
  FrameEvictionManager();
  ~FrameEvictionManager() override;

  // base::MemoryCoordinatorClient implementation:
  void OnPurgeMemory() override;

  void CullUnlockedFrames(size_t saved_frame_limit);

  void PurgeMemory(int percentage);

  friend struct base::DefaultSingletonTraits<FrameEvictionManager>;

  // Listens for system under pressure notifications and adjusts number of
  // cached frames accordingly.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::map<FrameEvictionManagerClient*, size_t> locked_frames_;
  std::list<FrameEvictionManagerClient*> unlocked_frames_;
  size_t max_number_of_saved_frames_;
  float max_handles_;

  DISALLOW_COPY_AND_ASSIGN(FrameEvictionManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_EVICTION_MANAGER_H_
