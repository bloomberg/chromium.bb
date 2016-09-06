// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/linux/blimp_display_manager.h"

#include "blimp/client/app/compositor/browser_compositor.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blimp_compositor_manager.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window.h"

namespace blimp {
namespace {
constexpr int kPointer1Id = 0;
constexpr int kPointer2Id = 1;
constexpr int kZoomOffsetMultiplier = 4;
}  // namespace

namespace client {

BlimpDisplayManager::BlimpDisplayManager(
    const gfx::Size& window_size,
    BlimpDisplayManagerDelegate* delegate,
    RenderWidgetFeature* render_widget_feature,
    TabControlFeature* tab_control_feature)
    : device_pixel_ratio_(1.f),
      delegate_(delegate),
      tab_control_feature_(tab_control_feature),
      platform_window_(new ui::X11Window(this)) {
  platform_window_->SetBounds(gfx::Rect(window_size));

  compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
      base::MakeUnique<CompositorDependenciesImpl>());

  compositor_ = base::MakeUnique<BrowserCompositor>(
      compositor_dependencies_->GetEmbedderDependencies());
  compositor_->SetSize(platform_window_->GetBounds().size());

  compositor_manager_ = base::MakeUnique<BlimpCompositorManager>(
      render_widget_feature, compositor_dependencies_.get());

  compositor_->SetContentLayer(compositor_manager_->layer());

  platform_window_->Show();

  tab_control_feature_->SetSizeAndScale(platform_window_->GetBounds().size(),
                                        device_pixel_ratio_);
}

BlimpDisplayManager::~BlimpDisplayManager() {}

void BlimpDisplayManager::OnBoundsChanged(const gfx::Rect& new_bounds) {
  compositor_->SetSize(new_bounds.size());
  tab_control_feature_->SetSizeAndScale(new_bounds.size(), device_pixel_ratio_);
}

void BlimpDisplayManager::DispatchEvent(ui::Event* event) {
  if (event->IsMouseWheelEvent()) {
    DispatchMouseWheelEvent(event->AsMouseWheelEvent());
  } else if (event->IsMouseEvent()) {
    DispatchMouseEvent(event->AsMouseEvent());
  }
}

void BlimpDisplayManager::DispatchMotionEventAura(
    ui::MotionEventAura* touch_event_stream,
    ui::EventType event_type,
    int pointer_id,
    int pointer_x,
    int pointer_y) {
  touch_event_stream->OnTouch(
      ui::TouchEvent(event_type, gfx::Point(pointer_x, pointer_y), pointer_id,
                     base::TimeTicks::Now()));
  compositor_manager_->OnTouchEvent(*touch_event_stream);
}

void BlimpDisplayManager::DispatchMouseWheelEvent(
    ui::MouseWheelEvent* wheel_event) {
  // In order to better simulate the Android experience on the Linux client
  // we convert mousewheel scrolling events into pinch/zoom events.
  bool zoom_out = wheel_event->y_offset() < 0;
  Zoom(wheel_event->x(), wheel_event->y(), wheel_event->y_offset(), zoom_out);
}

void BlimpDisplayManager::Zoom(int pointer_x,
                               int pointer_y,
                               int y_offset,
                               bool zoom_out) {
  int pinch_distance = abs(y_offset * kZoomOffsetMultiplier);
  int pointer2_y = pointer_y;
  if (zoom_out) {
    // Pointers start apart when zooming out.
    pointer2_y -= pinch_distance;
  }

  ui::MotionEventAura touch_event_stream;

  // Place two pointers.
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_PRESSED,
                          kPointer1Id, pointer_x, pointer_y);
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_PRESSED,
                          kPointer2Id, pointer_x, pointer2_y);

  // Pinch pointers. Need to simulate gradually in order for the event to be
  // properly processed.
  for (int pointer2_y_end = pointer2_y + pinch_distance;
       pointer2_y < pointer2_y_end; ++pointer2_y) {
    DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_MOVED,
                            kPointer2Id, pointer_x, pointer2_y);
  }

  // Remove pointers.
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_RELEASED,
                          kPointer1Id, pointer_x, pointer_y);
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_RELEASED,
                          kPointer2Id, pointer_x, pointer2_y);
}

ui::MotionEvent::Action MouseEventToAction(ui::MouseEvent* mouse_event) {
  switch (mouse_event->type()) {
    case ui::ET_MOUSE_PRESSED:
      return ui::MotionEvent::ACTION_DOWN;
    case ui::ET_MOUSE_RELEASED:
      return ui::MotionEvent::ACTION_UP;
    case ui::ET_MOUSE_DRAGGED:
      return ui::MotionEvent::ACTION_MOVE;
    default:
      return ui::MotionEvent::ACTION_NONE;
  }
}

void BlimpDisplayManager::DispatchMouseEvent(ui::MouseEvent* mouse_event) {
  ui::MotionEvent::Action action = MouseEventToAction(mouse_event);

  if (action != ui::MotionEvent::ACTION_NONE &&
      mouse_event->IsOnlyLeftMouseButton()) {
    ui::PointerProperties mouse_properties(mouse_event->x(), mouse_event->y(),
                                           0);
    ui::MotionEventGeneric motion_event(action, mouse_event->time_stamp(),
                                        mouse_properties);
    compositor_manager_->OnTouchEvent(motion_event);
  }
}

void BlimpDisplayManager::OnCloseRequest() {
  compositor_manager_->SetVisible(false);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  platform_window_->Close();
}

void BlimpDisplayManager::OnClosed() {
  if (delegate_)
    delegate_->OnClosed();
}

void BlimpDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  device_pixel_ratio_ = device_pixel_ratio;
  tab_control_feature_->SetSizeAndScale(platform_window_->GetBounds().size(),
                                        device_pixel_ratio_);

  if (widget != gfx::kNullAcceleratedWidget) {
    compositor_manager_->SetVisible(true);
    compositor_->SetAcceleratedWidget(widget);
  }
}

void BlimpDisplayManager::OnAcceleratedWidgetDestroyed() {
  compositor_manager_->SetVisible(false);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

}  // namespace client
}  // namespace blimp
