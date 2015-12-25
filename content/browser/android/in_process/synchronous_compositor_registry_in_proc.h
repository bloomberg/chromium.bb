// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_IN_PROC_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_IN_PROC_H_

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/renderer/android/synchronous_compositor_registry.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"

namespace ui {
class SynchronousInputHandlerProxy;
}

namespace content {

class SynchronousCompositorExternalBeginFrameSource;
class SynchronousCompositorImpl;
class SynchronousCompositorOutputSurface;

class SynchronousCompositorRegistryInProc
    : public SynchronousCompositorRegistry {
 public:
  static SynchronousCompositorRegistryInProc* GetInstance();

  void RegisterCompositor(int routing_id,
                          SynchronousCompositorImpl* compositor);
  void UnregisterCompositor(int routing_id,
                            SynchronousCompositorImpl* compositor);
  void RegisterInputHandler(
      int routing_id,
      ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy);
  void UnregisterInputHandler(int routing_id);

  // SynchronousCompositorRegistry overrides.
  void RegisterBeginFrameSource(int routing_id,
                                SynchronousCompositorExternalBeginFrameSource*
                                    begin_frame_source) override;
  void UnregisterBeginFrameSource(int routing_id,
                                  SynchronousCompositorExternalBeginFrameSource*
                                      begin_frame_source) override;
  void RegisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) override;
  void UnregisterOutputSurface(
      int routing_id,
      SynchronousCompositorOutputSurface* output_surface) override;

 private:
  friend struct base::DefaultLazyInstanceTraits<
      SynchronousCompositorRegistryInProc>;
  SynchronousCompositorRegistryInProc();
  ~SynchronousCompositorRegistryInProc() override;

  struct Entry {
    SynchronousCompositorImpl* compositor;
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source;
    SynchronousCompositorOutputSurface* output_surface;
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy;

    Entry();
    bool IsReady();
  };

  using EntryMap = base::hash_map<int, Entry>;

  void CheckIsReady(int routing_id);
  void UnregisterObjects(int routing_id);
  void RemoveEntryIfNeeded(int routing_id);
  bool CalledOnValidThread() const;

  EntryMap entry_map_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorRegistryInProc);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_REGISTRY_IN_PROC_H_
