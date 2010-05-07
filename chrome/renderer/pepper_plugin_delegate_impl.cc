// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_plugin_delegate_impl.h"

#include "app/surface/transport_dib.h"
#include "base/scoped_ptr.h"

namespace {

// Implements the Image2D using a TransportDIB.
class PlatformImage2DImpl : public pepper::PluginDelegate::PlatformImage2D {
 public:
  // This constructor will take ownership of the dib pointer.
  PlatformImage2DImpl(int width, int height, TransportDIB* dib)
      : width_(width),
        height_(height),
        dib_(dib) {
  }

  virtual skia::PlatformCanvas* Map() {
    return dib_->GetPlatformCanvas(width_, height_);
  }

  virtual intptr_t GetSharedMemoryHandle() const {
    return dib_->handle();
  }

 private:
  int width_;
  int height_;
  scoped_ptr<TransportDIB> dib_;

  DISALLOW_COPY_AND_ASSIGN(PlatformImage2DImpl);
};

}  // namespace

PepperPluginDelegateImpl::PepperPluginDelegateImpl(RenderView* render_view)
    : render_view_(render_view) {
}

pepper::PluginDelegate::PlatformImage2D*
PepperPluginDelegateImpl::CreateImage2D(int width, int height) {
  uint32 buffer_size = width * height * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code).  Note that the
  // TransportDIB is _not_ cached in the browser; this is because this memory
  // gets flushed by the renderer into another TransportDIB that represents the
  // page, which is then in turn flushed to the screen by the browser process.
  // When |transport_dib_| goes out of scope in the dtor, all of its shared
  // memory gets reclaimed.
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        false,
                                                        &dib_handle);
  if (!RenderThread::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid(dib_handle))
    return NULL;

  TransportDIB* dib = TransportDIB::Map(dib_handle);
#else
  static int next_dib_id = 0;
  TransportDIB* dib = TransportDIB::Create(buffer_size, next_dib_id++);
  if (!dib)
    return NULL;
#endif

  return new PlatformImage2DImpl(width, height, dib);
}
