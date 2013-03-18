// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/software_output_device.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

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

  virtual void Resize(gfx::Size size) OVERRIDE;
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

 private:
  void InitShaders();

  unsigned program_;
  unsigned vertex_shader_;
  unsigned fragment_shader_;
  unsigned vertex_buffer_;
  unsigned texture_id_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_GL_ADAPTER_H_
