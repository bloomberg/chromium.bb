// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ash/wm/window_finder.h"  // mash-ok
#include "ui/aura/mus/topmost_window_tracker.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"

namespace {

class WindowFinderClassic : public WindowFinder {
 public:
  WindowFinderClassic() {}

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    return ash::wm::GetTopmostWindowAtPoint(screen_point, ignore, nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowFinderClassic);
};

class WindowFinderMash : public WindowFinder {
 public:
  WindowFinderMash(TabDragController::EventSource event_source,
                   gfx::NativeWindow window) {
    ui::mojom::MoveLoopSource source =
        (event_source == TabDragController::EVENT_SOURCE_MOUSE)
            ? ui::mojom::MoveLoopSource::MOUSE
            : ui::mojom::MoveLoopSource::TOUCH;
    tracker_ = views::MusClient::Get()
                   ->window_tree_client()
                   ->StartObservingTopmostWindow(source, window);
  }

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override {
    DCHECK_LE(ignore.size(), 1u);
    aura::Window* window =
        ignore.empty() ? tracker_->topmost() : tracker_->second_topmost();
    if (!window)
      return nullptr;
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    if (!widget)
      return nullptr;
    return widget->GetNativeWindow();
  }

 private:
  std::unique_ptr<aura::TopmostWindowTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(WindowFinderMash);
};

}  // namespace

std::unique_ptr<WindowFinder> WindowFinder::Create(
    TabDragController::EventSource source,
    gfx::NativeWindow window) {
  if (features::IsAshInBrowserProcess())
    return std::make_unique<WindowFinderClassic>();
  return std::make_unique<WindowFinderMash>(source, window);
}

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  NOTREACHED();
  return nullptr;
}
