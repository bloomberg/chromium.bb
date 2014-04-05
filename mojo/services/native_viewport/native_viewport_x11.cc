// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_types.h"

namespace mojo {
namespace services {

class NativeViewportX11 : public NativeViewport,
                          public ui::PlatformEventDispatcher {
 public:
  NativeViewportX11(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }

  virtual ~NativeViewportX11() {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

    XDestroyWindow(gfx::GetXDisplay(), window_);
  }

 private:
  // Overridden from NativeViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE {
    XDisplay* display = gfx::GetXDisplay();

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.override_redirect = False;

    bounds_ = bounds;
    window_ = XCreateWindow(
        display,
        DefaultRootWindow(display),
        bounds_.x(), bounds_.y(), bounds_.width(), bounds_.height(),
        0,  // border width
        CopyFromParent,  // depth
        InputOutput,
        CopyFromParent,  // visual
        CWBackPixmap | CWOverrideRedirect,
        &swa);

    atom_wm_protocols_ = XInternAtom(display, "WM_PROTOCOLS", 1);
    atom_wm_delete_window_ = XInternAtom(display, "WM_DELETE_WINDOW", 1);
    XSetWMProtocols(display, window_, &atom_wm_delete_window_, 1);

    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

    delegate_->OnAcceleratedWidgetAvailable(window_);
  }

  virtual void Show() OVERRIDE {
    XDisplay* display = gfx::GetXDisplay();
    XMapWindow(display, window_);
    XFlush(display);
  }

  virtual void Hide() OVERRIDE {
    XWithdrawWindow(gfx::GetXDisplay(), window_, 0);
  }

  virtual void Close() OVERRIDE {
    // TODO(beng): perform this in response to XWindow destruction.
    delegate_->OnDestroyed();
  }

  virtual gfx::Size GetSize() OVERRIDE {
    return bounds_.size();
  }

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void SetCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void ReleaseCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return event->type == ClientMessage &&
           event->xclient.message_type == atom_wm_protocols_;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    Atom protocol = static_cast<Atom>(event->xclient.data.l[0]);
    if (protocol == atom_wm_delete_window_)
      delegate_->OnDestroyed();
    return ui::POST_DISPATCH_NONE;
  }

  NativeViewportDelegate* delegate_;
  gfx::Rect bounds_;
  XID window_;
  Atom atom_wm_protocols_;
  Atom atom_wm_delete_window_;

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
