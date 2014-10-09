// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/io_surface_context_mac.h"

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace content {

// static
scoped_refptr<IOSurfaceContext>
IOSurfaceContext::Get() {
  TRACE_EVENT0("browser", "IOSurfaceContext::Get");

  // Return the context, if it exists.
  if (current_context_)
    return current_context_;

  base::ScopedTypeRef<CGLContextObj> cgl_context;
  CGLError error = kCGLNoError;

  // Create the pixel format object for the context.
  std::vector<CGLPixelFormatAttribute> attribs;
  attribs.push_back(kCGLPFADepthSize);
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(1));
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  GLint number_virtual_screens = 0;
  base::ScopedTypeRef<CGLPixelFormatObj> pixel_format;
  error = CGLChoosePixelFormat(&attribs.front(),
                               pixel_format.InitializeInto(),
                               &number_virtual_screens);
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create pixel format object.";
    return NULL;
  }

  error = CGLCreateContext(pixel_format, NULL, cgl_context.InitializeInto());
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create context object.";
    return NULL;
  }

  return new IOSurfaceContext(cgl_context);
}

void IOSurfaceContext::PoisonContextAndSharegroup() {
  if (poisoned_)
    return;
  DCHECK(current_context_ == this);
  current_context_ = NULL;
  poisoned_ = true;
}

IOSurfaceContext::IOSurfaceContext(
    base::ScopedTypeRef<CGLContextObj> cgl_context)
    : cgl_context_(cgl_context), poisoned_(false) {
  DCHECK(!current_context_);
  current_context_ = this;
  GpuDataManager::GetInstance()->AddObserver(this);
}

IOSurfaceContext::~IOSurfaceContext() {
  GpuDataManager::GetInstance()->RemoveObserver(this);

  if (!poisoned_) {
    DCHECK(current_context_ == this);
    current_context_ = NULL;
  } else {
    DCHECK(current_context_ != this);
  }
}

void IOSurfaceContext::OnGpuSwitching() {
  // Recreate all browser-side GL contexts whenever the GPU switches. If this
  // is not done, performance will suffer.
  // http://crbug.com/361493
  PoisonContextAndSharegroup();
}

// static
IOSurfaceContext* IOSurfaceContext::current_context_ = NULL;

}  // namespace content
