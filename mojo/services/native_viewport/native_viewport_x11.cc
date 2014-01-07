// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_x11.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_types.h"

namespace mojo {
namespace services {

class NativeViewportX11 : public NativeViewport,
                          public base::MessagePumpDispatcher {
 public:
  NativeViewportX11(NativeViewportDelegate* delegate)
      : delegate_(delegate),
        bounds_(10, 10, 800, 600) {
  }

  virtual ~NativeViewportX11() {
    base::MessagePumpX11::Current()->RemoveDispatcherForRootWindow(this);
    base::MessagePumpX11::Current()->RemoveDispatcherForWindow(window_);

    XDestroyWindow(gfx::GetXDisplay(), window_);
  }

 private:
  // Overridden from NativeViewport:

  virtual gfx::Size GetSize() OVERRIDE {
    return bounds_.size();
  }

  virtual void Init() OVERRIDE {
    XDisplay* display = gfx::GetXDisplay();
    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.override_redirect = False;
    window_ = XCreateWindow(
        display,
        DefaultRootWindow(display),
        bounds_.x(), bounds_.y(), bounds_.width(), bounds_.height(),
        0,  // border width
        CopyFromParent,  // depth
        InputOutput,
        CopyFromParent,  // visual
        CWBackPixmap | CWOverrideRedirect, &swa);

    base::MessagePumpX11::Current()->AddDispatcherForWindow(this, window_);
    base::MessagePumpX11::Current()->AddDispatcherForRootWindow(this);

    XMapWindow(display, window_);
    XFlush(display);

    delegate_->OnAcceleratedWidgetAvailable(window_);
  }

  virtual void Close() OVERRIDE {
    // TODO(beng): perform this in response to XWindow destruction.
    delegate_->OnDestroyed();
  }

  virtual void SetCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void ReleaseCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  // Overridden from base::MessagePumpDispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE {
    return true;
  }

  NativeViewportDelegate* delegate_;
  gfx::Rect bounds_;
  XID window_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportX11);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportX11(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
