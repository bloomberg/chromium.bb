// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "cc/software_output_device.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gfx/size.h"

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class CompositorSoftwareOutputDeviceGLAdapter
    : NON_EXPORTED_BASE(public cc::SoftwareOutputDevice),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
public:
  CompositorSoftwareOutputDeviceGLAdapter(
      WebKit::WebGraphicsContext3D* context3d);
  virtual ~CompositorSoftwareOutputDeviceGLAdapter();

  // cc::SoftwareOutputDevice implementation
  virtual WebKit::WebImage* Lock(bool forWrite) OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual void DidChangeViewportSize(gfx::Size size) OVERRIDE;

private:
  void Initialize();
  void Destroy();
  void Resize(const gfx::Size& viewportSize);
  void Draw(void* pixels);

  bool initialized_;
  int program_;
  int vertex_shader_;
  int fragment_shader_;
  unsigned int vertex_buffer_;
  unsigned framebuffer_texture_id_;
  gfx::Size framebuffer_texture_size_;

  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<SkDevice> device_;
  WebKit::WebImage image_;
  bool locked_for_write_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_
