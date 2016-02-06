// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_GLOBAL_STATE_H_
#define COMPONENTS_HTML_VIEWER_GLOBAL_STATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/html_viewer/blink_settings.h"
#include "components/html_viewer/discardable_memory_allocator.h"
#include "components/mus/gles2/mojo_gpu_memory_buffer_manager.h"
#include "components/mus/gles2/raster_thread_helper.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/size.h"

namespace font_service {
class FontLoader;
}

namespace mojo {
class ApplicationImpl;
namespace shell {
namespace mojom {
class Shell;
}
}
}

namespace ui {
namespace mojo {
class UIInit;
}
}

namespace scheduler {
class RendererScheduler;
}

namespace html_viewer {

class BlinkPlatformImpl;
class MediaFactory;

// GlobalState encapsulates the necessary state needed by HTMLViewer. Some
// objects are created immediately in the constructor, otherwise not until
// InitIfNecessary() is invoked.
//
// GlobalState can be initialized in two distinct ways:
// . headless: this is determined by the command line, but can be forced by way
//   of InitHeadless().
// . with a ui: this is done via InitIfNecessary().
class GlobalState {
 public:
  explicit GlobalState(mojo::ApplicationImpl* app);
  ~GlobalState();

  // Inits with the specified screen size and device pixel ratio.
  // NOTE: we wait to complete setup until the device pixel ratio is available
  // as ResourceBundle uses the device pixel ratio during initialization.
  void InitIfNecessary(const gfx::Size& screen_size_in_pixels,
                       float device_pixel_ratio);

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

  mus::MojoGpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return &gpu_memory_buffer_manager_;
  }

  const mus::mojom::GpuInfo* GetGpuInfo();

  MediaFactory* media_factory() { return media_factory_.get(); }

  BlinkSettings* blink_settings() { return blink_settings_.get(); }
  void set_blink_settings(BlinkSettings* blink_settings) {
    blink_settings_.reset(blink_settings);
  }

 private:
  // Callback for |gpu_service_|->GetGpuInfo().
  void GetGpuInfoCallback(mus::mojom::GpuInfoPtr gpu_info);

  // App for HTMLViewer, not the document's app.
  // WARNING: do not expose this. It's too easy to use the wrong one.
  // HTMLDocument should be using the application it creates, not this one.
  mojo::ApplicationImpl* app_;

  resource_provider::ResourceLoader resource_loader_;

  // True once we've completed init.
  bool did_init_;

  float device_pixel_ratio_;

  gfx::Size screen_size_in_pixels_;

  scoped_ptr<ui::mojo::UIInit> ui_init_;

  // Skia requires that we have one of these. Unlike the one used in chrome,
  // this doesn't use purgable shared memory. Instead, it tries to free the
  // oldest unlocked chunks on allocation.
  //
  // TODO(erg): In the long run, delete this allocator and get the real shared
  // memory based purging allocator working here.
  DiscardableMemoryAllocator discardable_memory_allocator_;

  mojo::TracingImpl tracing_;

  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler_;
  scoped_ptr<BlinkPlatformImpl> blink_platform_;
  base::Thread compositor_thread_;
  gles2::RasterThreadHelper raster_thread_helper_;
  mus::MojoGpuMemoryBufferManager gpu_memory_buffer_manager_;
  mus::mojom::GpuPtr gpu_service_;
  mus::mojom::GpuInfoPtr gpu_info_;
  scoped_ptr<MediaFactory> media_factory_;

  scoped_ptr<BlinkSettings> blink_settings_;

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  skia::RefPtr<font_service::FontLoader> font_loader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GlobalState);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_GLOBAL_STATE_H_
