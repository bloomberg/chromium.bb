// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_image_2d_impl.h"

#include "build/build_config.h"

#include "base/metrics/histogram.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/surface/transport_dib.h"

namespace content {

PepperPlatformImage2DImpl::PepperPlatformImage2DImpl(int width,
                                                     int height,
                                                     TransportDIB* dib)
    : width_(width),
      height_(height),
      dib_(dib) {
}

PepperPlatformImage2DImpl::~PepperPlatformImage2DImpl() {
}

// static
PepperPlatformImage2DImpl* PepperPlatformImage2DImpl::Create(int width,
                                                             int height) {
  uint32 buffer_size = width * height * 4;
  UMA_HISTOGRAM_COUNTS("Plugin.PepperImage2DSize", buffer_size);

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code).
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        false,
                                                        &dib_handle);
  if (!RenderThreadImpl::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid_handle(dib_handle))
    return NULL;

  TransportDIB* dib = TransportDIB::Map(dib_handle);
#else
  static int next_dib_id = 0;
  TransportDIB* dib = TransportDIB::Create(buffer_size, next_dib_id++);
  if (!dib)
    return NULL;
#endif

  return new PepperPlatformImage2DImpl(width, height, dib);
}

skia::PlatformCanvas* PepperPlatformImage2DImpl::Map() {
  return dib_->GetPlatformCanvas(width_, height_);
}

intptr_t PepperPlatformImage2DImpl::GetSharedMemoryHandle(
    uint32* byte_count) const {
  *byte_count = dib_->size();
#if defined(OS_WIN)
  return reinterpret_cast<intptr_t>(dib_->handle());
#elif defined(OS_MACOSX) || defined(OS_ANDROID)
  return static_cast<intptr_t>(dib_->handle().fd);
#elif defined(OS_POSIX)
  return static_cast<intptr_t>(dib_->handle());
#endif
}

TransportDIB* PepperPlatformImage2DImpl::GetTransportDIB() const {
  return dib_.get();
}

}  // namespace content
