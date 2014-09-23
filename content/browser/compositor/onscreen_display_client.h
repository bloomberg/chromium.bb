// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_
#define CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_

#include "cc/surfaces/display_client.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/surfaces/display.h"

namespace cc {
class ContextProvider;
class SurfaceManager;
}

namespace content {
class SurfaceDisplayOutputSurface;

// This class provides a DisplayClient implementation for drawing directly to an
// onscreen context.
class OnscreenDisplayClient : cc::DisplayClient {
 public:
  OnscreenDisplayClient(
      scoped_ptr<cc::OutputSurface> output_surface,
      cc::SurfaceManager* manager,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~OnscreenDisplayClient();

  cc::Display* display() { return display_.get(); }
  void set_surface_output_surface(SurfaceDisplayOutputSurface* surface) {
    surface_display_output_surface_ = surface;
  }

  // cc::DisplayClient implementation.
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual void DisplayDamaged() OVERRIDE;
  virtual void DidSwapBuffers() OVERRIDE;
  virtual void DidSwapBuffersComplete() OVERRIDE;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE;

 private:
  void ScheduleDraw();
  void Draw();

  scoped_ptr<cc::OutputSurface> output_surface_;
  scoped_ptr<cc::Display> display_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  SurfaceDisplayOutputSurface* surface_display_output_surface_;
  bool scheduled_draw_;
  // True if a draw should be scheduled, but it's hit the limit on max frames
  // pending.
  bool deferred_draw_;
  int pending_frames_;

  base::WeakPtrFactory<OnscreenDisplayClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OnscreenDisplayClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_
