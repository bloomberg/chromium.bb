// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"

namespace content {

class SynchronousCompositorExternalBeginFrameSource;
class SynchronousCompositorImpl;
class SynchronousCompositorOutputSurface;

class SynchronousCompositorRegistry {
 public:
  static SynchronousCompositorRegistry* GetInstance();

  void RegisterCompositor(int routing_id,
                          SynchronousCompositorImpl* compositor);
  void UnregisterCompositor(int routing_id,
                            SynchronousCompositorImpl* compositor);
  void RegisterBeginFrameSource(
      int routing_id,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source);
  void UnregisterBeginFrameSource(
      int routing_id,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source);
  void RegisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface);
  void UnregisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface);

 private:
  friend struct base::DefaultLazyInstanceTraits<SynchronousCompositorRegistry>;
  SynchronousCompositorRegistry();
  ~SynchronousCompositorRegistry();

  struct Entry {
    SynchronousCompositorImpl* compositor;
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source;
    SynchronousCompositorOutputSurface* output_surface;

    Entry();
    bool IsReady();
  };

  using EntryMap = base::hash_map<int, Entry>;

  void CheckIsReady(int routing_id);
  void UnregisterObjects(int routing_id);
  void RemoveEntryIfNeeded(int routing_id);
  bool CalledOnValidThread() const;

  EntryMap entry_map_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
