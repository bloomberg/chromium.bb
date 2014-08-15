// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window.h"

namespace mojo {

class PlatformViewportX11 : public PlatformViewport,
                            public ui::PlatformWindowDelegate {
 public:
  explicit PlatformViewportX11(Delegate* delegate) : delegate_(delegate) {
  }

  virtual ~PlatformViewportX11() {
    // Destroy the platform-window while |this| is still alive.
    platform_window_.reset();
  }

 private:
  // Overridden from PlatformViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE {
    CHECK(!event_source_);
    CHECK(!platform_window_);

    event_source_ = ui::PlatformEventSource::CreateDefault();

    platform_window_.reset(new ui::X11Window(this));
    platform_window_->SetBounds(bounds);
  }

  virtual void Show() OVERRIDE {
    platform_window_->Show();
  }

  virtual void Hide() OVERRIDE {
    platform_window_->Hide();
  }

  virtual void Close() OVERRIDE {
    platform_window_->Close();
  }

  virtual gfx::Size GetSize() OVERRIDE {
    return bounds_.size();
  }

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    platform_window_->SetBounds(bounds);
  }

  virtual void SetCapture() OVERRIDE {
    platform_window_->SetCapture();
  }

  virtual void ReleaseCapture() OVERRIDE {
    platform_window_->ReleaseCapture();
  }

  // ui::PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) OVERRIDE {
    bounds_ = new_bounds;
    delegate_->OnBoundsChanged(new_bounds);
  }

  virtual void OnDamageRect(const gfx::Rect& damaged_region) OVERRIDE {
  }

  virtual void DispatchEvent(ui::Event* event) OVERRIDE {
    delegate_->OnEvent(event);
  }

  virtual void OnCloseRequest() OVERRIDE {
    platform_window_->Close();
  }

  virtual void OnClosed() OVERRIDE {
    delegate_->OnDestroyed();
  }

  virtual void OnWindowStateChanged(ui::PlatformWindowState state) OVERRIDE {
  }

  virtual void OnLostCapture() OVERRIDE {
  }

  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    delegate_->OnAcceleratedWidgetAvailable(widget);
  }

  virtual void OnActivationChanged(bool active) OVERRIDE {}

  scoped_ptr<ui::PlatformEventSource> event_source_;
  scoped_ptr<ui::PlatformWindow> platform_window_;
  Delegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportX11);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(new PlatformViewportX11(delegate)).Pass();
}

}  // namespace mojo
