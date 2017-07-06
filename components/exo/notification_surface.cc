// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification_surface.h"

#include "components/exo/notification_surface_manager.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace exo {

namespace {

class CustomWindowDelegate : public aura::WindowDelegate {
 public:
  explicit CustomWindowDelegate(NotificationSurface* notification_surface)
      : notification_surface_(notification_surface) {}
  ~CustomWindowDelegate() override {}

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return notification_surface_->GetCursor(point);
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override {
    return notification_surface_->HasHitTestMask();
  }
  void GetHitTestMask(gfx::Path* mask) const override {
    notification_surface_->GetHitTestMask(mask);
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Propagates the key event upto the top-level views Widget so that we can
    // trigger proper events in the views/ash level there. Event handling for
    // Surfaces is done in a post event handler in keyboard.cc.
    views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
        notification_surface_->host_window());
    if (widget)
      widget->OnKeyEvent(event);
  }

 private:
  NotificationSurface* const notification_surface_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowDelegate);
};

}  // namespace

NotificationSurface::NotificationSurface(NotificationSurfaceManager* manager,
                                         Surface* surface,
                                         const std::string& notification_key)
    : SurfaceTreeHost("ExoNotificationSurface", new CustomWindowDelegate(this)),
      manager_(manager),
      notification_key_(notification_key) {
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->Show();
}

NotificationSurface::~NotificationSurface() {
  if (added_to_manager_)
    manager_->RemoveSurface(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

const gfx::Size& NotificationSurface::GetContentSize() const {
  return root_surface()->content_size();
}

void NotificationSurface::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();

  gfx::Rect bounds = host_window()->bounds();
  auto& size = host_window()->bounds().size();
  if (bounds.size() != size) {
    bounds.set_size(size);
    host_window()->SetBounds(bounds);
  }

  // Defer AddSurface until there are contents to show.
  if (!added_to_manager_ && !size.IsEmpty()) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }
}

void NotificationSurface::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(surface, root_surface());
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);
}

}  // namespace exo
