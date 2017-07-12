// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/latency/latency_info.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
enum class SwapResult;
}

namespace ui {
class Compositor;
class ContextFactory;
class ContextFactoryPrivate;
}

namespace viz {
class GLHelper;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace content {

// This class provides the interface for creating the support for the
// cross-process image transport, both for creating the shared surface handle
// (destination surface for the GPU process) and the transport client (logic for
// using that surface as a texture). The factory is a process-wide singleton.
class CONTENT_EXPORT ImageTransportFactory {
 public:
  virtual ~ImageTransportFactory() {}

  // Initializes the global transport factory.
  static void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner);

  // Initializes the global transport factory for unit tests using the provided
  // context factory.
  static void InitializeForUnitTests(
      std::unique_ptr<ImageTransportFactory> factory);

  // Terminates the global transport factory.
  static void Terminate();

  // Gets the factory instance.
  static ImageTransportFactory* GetInstance();

  // Gets the image transport factory as a context factory for the compositor.
  virtual ui::ContextFactory* GetContextFactory() = 0;

  // Gets the image transport factory as the privileged context factory for the
  // compositor. TODO(fsamuel): This interface should eventually go away once
  // Mus subsumes this functionality.
  virtual ui::ContextFactoryPrivate* GetContextFactoryPrivate() = 0;

  // Gets a GLHelper instance, associated with the shared context. This
  // GLHelper will get destroyed whenever the shared context is lost
  // (ImageTransportFactoryObserver::OnLostResources is called).
  virtual viz::GLHelper* GetGLHelper() = 0;

  virtual void SetGpuChannelEstablishFactory(
      gpu::GpuChannelEstablishFactory* factory) = 0;

#if defined(OS_MACOSX)
  // Called with |suspended| as true when the ui::Compositor has been
  // disconnected from an NSView and may be attached to another one. Called
  // with |suspended| as false after the ui::Compositor has been connected to
  // a new NSView and the first commit targeted at the new NSView has
  // completed. This ensures that content and frames intended for the old
  // NSView will not flash in the new NSView.
  virtual void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                                bool suspended) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
