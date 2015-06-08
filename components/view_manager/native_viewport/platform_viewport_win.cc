// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/native_viewport/platform_viewport.h"

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/native_viewport/platform_viewport_headless.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace native_viewport {
namespace {
float ConvertUIWheelValueToMojoValue(int offset) {
  // Mojo's event type takes a value between -1 and 1. Normalize by allowing
  // up to 20 of ui's offset. This is a bit arbitrary.
  return std::max(
      -1.0f, std::min(1.0f, static_cast<float>(offset) /
                                (20 * static_cast<float>(
                                          ui::MouseWheelEvent::kWheelDelta))));
}
}  // namespace

class PlatformViewportWin : public PlatformViewport,
                            public ui::PlatformWindowDelegate {
 public:
  explicit PlatformViewportWin(Delegate* delegate)
      : delegate_(delegate) {
  }

  ~PlatformViewportWin() override {
    // Destroy the platform-window while |this| is still alive.
    platform_window_.reset();
  }

 private:
  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override {
    metrics_ = mojo::ViewportMetrics::New();
    // TODO(sky): make density real.
    metrics_->device_pixel_ratio = 1.f;
    metrics_->size_in_pixels = mojo::Size::From(bounds.size());
    platform_window_.reset(new ui::WinWindow(this, bounds));
  }

  void Show() override {
    platform_window_->Show();
  }

  void Hide() override {
    platform_window_->Hide();
  }

  void Close() override {
    platform_window_->Close();
  }

  gfx::Size GetSize() override {
    return metrics_->size_in_pixels.To<gfx::Size>();
  }

  void SetBounds(const gfx::Rect& bounds) override {
    platform_window_->SetBounds(bounds);
  }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {
    // TODO(fsamuel): Use the real device_scale_factor.
    delegate_->OnMetricsChanged(new_bounds.size(),
                                1.f /* device_scale_factor */);
  }

  void OnDamageRect(const gfx::Rect& damaged_region) override {
  }

  void DispatchEvent(ui::Event* event) override {
    // TODO(jam): this code is copied from the X11 version.
    mojo::EventPtr mojo_event(mojo::Event::From(*event));
    if (event->IsMouseWheelEvent()) {
      // Mojo's event type has a different meaning for wheel events. Convert
      // between the two.
      ui::MouseWheelEvent* wheel_event =
          static_cast<ui::MouseWheelEvent*>(event);
      DCHECK(mojo_event->pointer_data);
      mojo_event->pointer_data->horizontal_wheel =
          ConvertUIWheelValueToMojoValue(wheel_event->x_offset());
      mojo_event->pointer_data->horizontal_wheel =
          ConvertUIWheelValueToMojoValue(wheel_event->y_offset());
    }
    delegate_->OnEvent(mojo_event.Pass());

    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_TOUCH_PRESSED:
        platform_window_->SetCapture();
        break;
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_TOUCH_RELEASED:
        platform_window_->ReleaseCapture();
        break;
      default:
        break;
    }
  }

  void OnCloseRequest() override {
    platform_window_->Close();
  }

  void OnClosed() override {
    delegate_->OnDestroyed();
  }

  void OnWindowStateChanged(ui::PlatformWindowState state) override {
  }

  void OnLostCapture() override {
  }

  void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) override {
    delegate_->OnAcceleratedWidgetAvailable(widget,
                                            metrics_->device_pixel_ratio);
  }

  void OnActivationChanged(bool active) override {}

  scoped_ptr<ui::PlatformWindow> platform_window_;
  Delegate* delegate_;
  mojo::ViewportMetricsPtr metrics_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportWin);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate,
                                                      bool headless) {
  if (headless)
    return PlatformViewportHeadless::Create(delegate);
  return make_scoped_ptr(new PlatformViewportWin(delegate));
}

}  // namespace native_viewport
