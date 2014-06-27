// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/event_factory_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace mojo {
namespace services {

bool override_redirect = false;

class NativeViewportOzone : public NativeViewport,
                            public ui::PlatformEventDispatcher {
 public:
  NativeViewportOzone(NativeViewportDelegate* delegate)
      : delegate_(delegate),
        widget_(gfx::kNullAcceleratedWidget) {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::SurfaceFactoryOzone::GetInstance();
    widget_ = surface_factory->GetAcceleratedWidget();
    // TODO(sky): need to enable this.
    // ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  }

  virtual ~NativeViewportOzone() {
    // TODO(sky): need to enable this.
    // ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(
    // this);
  }

 private:
  // Overridden from NativeViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE {
    NOTIMPLEMENTED();
    bounds_ = bounds;
    delegate_->OnAcceleratedWidgetAvailable(widget_);
  }

  virtual void Show() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void Hide() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void Close() OVERRIDE {
    delegate_->OnDestroyed();
  }

  virtual gfx::Size GetSize() OVERRIDE {
    return bounds_.size();
  }

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    bounds_ = bounds;
    NOTIMPLEMENTED();
  }

  virtual void SetCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void ReleaseCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& ne) OVERRIDE {
    CHECK(ne);
    ui::Event* event = static_cast<ui::Event*>(ne);
    if (event->IsMouseEvent() || event->IsScrollEvent()) {
      return ui::CursorFactoryOzone::GetInstance()->GetCursorWindow() ==
          widget_;
    }

    return true;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& ne) OVERRIDE {
    ui::Event* event = static_cast<ui::Event*>(ne);
    delegate_->OnEvent(event);
    return ui::POST_DISPATCH_STOP_PROPAGATION;
  }

  NativeViewportDelegate* delegate_;
  gfx::AcceleratedWidget widget_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportOzone);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportOzone(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
