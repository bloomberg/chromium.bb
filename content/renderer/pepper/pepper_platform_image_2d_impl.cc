// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_image_2d_impl.h"

#include "build/build_config.h"
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

// On Mac, we have to tell the browser to free the transport DIB.
PepperPlatformImage2DImpl::~PepperPlatformImage2DImpl() {
#if defined(OS_MACOSX)
  if (dib_.get()) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_FreeTransportDIB(dib_->id()));
  }
#endif
}

// static
PepperPlatformImage2DImpl* PepperPlatformImage2DImpl::Create(int width,
                                                             int height) {
  uint32 buffer_size = width * height * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code). The TransportDIB
  // is cached in the browser, and is freed (in typical cases) by the
  // PepperPlatformImage2DImpl's destructor.
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        true,
                                                        &dib_handle);
  if (!RenderThreadImpl::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid_handle(dib_handle))
    return NULL;

  TransportDIB* dib = TransportDIB::CreateWithHandle(dib_handle);
#else
  static int next_dib_id = 0;
  TransportDIB* dib = TransportDIB::Create(buffer_size, next_dib_id++);
  if (!dib)
    return NULL;
#endif

  return new PepperPlatformImage2DImpl(width, height, dib);
}

SkCanvas* PepperPlatformImage2DImpl::Map() {
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
