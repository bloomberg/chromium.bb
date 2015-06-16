// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_SETUP_H_
#define COMPONENTS_HTML_VIEWER_SETUP_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "components/html_viewer/discardable_memory_allocator.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "components/view_manager/gles2/mojo_gpu_memory_buffer_manager.h"
#include "components/view_manager/gles2/raster_thread_helper.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {
class ApplicationImpl;
class Shell;
}

namespace scheduler {
class RendererScheduler;
}

namespace html_viewer {

class BlinkPlatformImpl;
class UISetup;
class MediaFactory;

// Setup encapsulates the necessary state needed by HTMLViewer. Some objects
// are created immediately in the constructor, otherwise not until
// InitIfNecessary() is invoked.
// Setup can be initialized in two distinct ways:
// . headless: this is determined by the command line, but can be forced by way
//   of InitHeadless().
// . with a ui: this is done via InitIfNecessary().
class Setup {
 public:
  explicit Setup(mojo::ApplicationImpl* app);
  ~Setup();

  // Inits with the specified screen size and device pixel ratio.
  // NOTE: we wait to complete setup until the device pixel ratio is available
  // as ResourceBundle uses the device pixel ratio during initialization.
  void InitIfNecessary(const gfx::Size& screen_size_in_pixels,
                       float device_pixel_ratio);

  bool is_headless() const { return is_headless_; }
  bool did_init() const { return did_init_; }

  const gfx::Size& screen_size_in_pixels() const {
    return screen_size_in_pixels_;
  }

  float device_pixel_ratio() const { return device_pixel_ratio_; }

  scoped_refptr<base::SingleThreadTaskRunner> compositor_thread() {
    return compositor_thread_.task_runner();
  }

  gles2::RasterThreadHelper* raster_thread_helper() {
    return &raster_thread_helper_;
  }

  gles2::MojoGpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return &gpu_memory_buffer_manager_;
  }

  MediaFactory* media_factory() { return media_factory_.get(); }

 private:
  // App for HTMLViewer, not the document's app.
  // WARNING: do not expose this. It's too easy to use the wrong one.
  // HTMLDocument should be using the application it creates, not this one.
  mojo::ApplicationImpl* app_;

  resource_provider::ResourceLoader resource_loader_;

  const bool is_headless_;

  // True once we've completed init.
  bool did_init_;

  float device_pixel_ratio_;

  gfx::Size screen_size_in_pixels_;

  scoped_ptr<UISetup> ui_setup_;

  // Skia requires that we have one of these. Unlike the one used in chrome,
  // this doesn't use purgable shared memory. Instead, it tries to free the
  // oldest unlocked chunks on allocation.
  //
  // TODO(erg): In the long run, delete this allocator and get the real shared
  // memory based purging allocator working here.
  DiscardableMemoryAllocator discardable_memory_allocator_;

  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler_;
  scoped_ptr<BlinkPlatformImpl> blink_platform_;
  base::Thread compositor_thread_;
  gles2::RasterThreadHelper raster_thread_helper_;
  gles2::MojoGpuMemoryBufferManager gpu_memory_buffer_manager_;
  scoped_ptr<MediaFactory> media_factory_;

  DISALLOW_COPY_AND_ASSIGN(Setup);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_SETUP_H_
