// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/partial_screenshot_controller.h"

#include <cmath>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/stl_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

// The size to increase the invalidated area in the layer to repaint. The area
// should be slightly bigger than the actual region because the region indicator
// rectangles are drawn outside of the selected region.
const int kInvalidateRegionAdditionalSize = 3;

// This will prevent the user from taking a screenshot across multiple
// monitors. it will stop the mouse at the any edge of the screen. must
// swtich back on when the screenshot is complete.
void EnableMouseWarp(bool enable) {
  Shell::GetInstance()->mouse_cursor_filter()->set_mouse_warp_enabled(enable);
}

}  // namespace

class PartialScreenshotController::PartialScreenshotLayer
    : public ui::LayerOwner,
      public ui::LayerDelegate {
 public:
  PartialScreenshotLayer(ui::Layer* parent) {
    SetLayer(new ui::Layer(ui::LAYER_TEXTURED));
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBounds(parent->bounds());
    parent->Add(layer());
    parent->StackAtTop(layer());
    layer()->SetVisible(true);
    layer()->set_delegate(this);
  }
  ~PartialScreenshotLayer() override {}

  const gfx::Rect& region() const { return region_; }

  void SetRegion(const gfx::Rect& region) {
    // Invalidates the region which covers the current and new region.
    gfx::Rect union_rect(region_);
    union_rect.Union(region);
    union_rect.Inset(-kInvalidateRegionAdditionalSize,
                     -kInvalidateRegionAdditionalSize);
    union_rect.Intersects(layer()->bounds());

    region_ = region;
    layer()->SchedulePaint(union_rect);
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    if (region_.IsEmpty())
      return;

    // Screenshot area representation: black rectangle with white
    // rectangle inside.  To avoid capturing these rectangles when mouse
    // release, they should be outside of the actual capturing area.
    gfx::Rect rect(region_);
    ui::PaintRecorder recorder(context, layer()->size());
    rect.Inset(-1, -1);
    recorder.canvas()->DrawRect(rect, SK_ColorWHITE);
    rect.Inset(-1, -1);
    recorder.canvas()->DrawRect(rect, SK_ColorBLACK);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  base::Closure PrepareForLayerBoundsChange() override {
    return base::Closure();
  }

  gfx::Rect region_;

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotLayer);
};

class PartialScreenshotController::ScopedCursorSetter {
 public:
  ScopedCursorSetter(::wm::CursorManager* cursor_manager,
                     gfx::NativeCursor cursor)
      : cursor_manager_(nullptr) {
    if (cursor_manager->IsCursorLocked())
      return;
    gfx::NativeCursor original_cursor = cursor_manager->GetCursor();
    cursor_manager_ = cursor_manager;
    cursor_manager_->SetCursor(cursor);
    if (!cursor_manager_->IsCursorVisible())
      cursor_manager_->ShowCursor();
    cursor_manager_->LockCursor();
    // SetCursor does not make any effects at this point but it sets back to
    // the original cursor when unlocked.
    cursor_manager_->SetCursor(original_cursor);
  }

  ~ScopedCursorSetter() {
    if (cursor_manager_)
      cursor_manager_->UnlockCursor();
  }

 private:
  ::wm::CursorManager* cursor_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorSetter);
};

PartialScreenshotController::PartialScreenshotController()
    : root_window_(nullptr), screenshot_delegate_(nullptr) {
  // Keep this here and don't move it to StartPartialScreenshotSession(), as it
  // needs to be pre-pended by MouseCursorEventFilter in Shell::Init().
  Shell::GetInstance()->PrependPreTargetHandler(this);
}

PartialScreenshotController::~PartialScreenshotController() {
  if (screenshot_delegate_)
    Cancel();
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void PartialScreenshotController::StartPartialScreenshotSession(
    ScreenshotDelegate* screenshot_delegate) {
  // Already in a screenshot session.
  if (screenshot_delegate_) {
    DCHECK_EQ(screenshot_delegate_, screenshot_delegate);
    return;
  }

  screenshot_delegate_ = screenshot_delegate;
  gfx::Screen::GetScreen()->AddObserver(this);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    layers_[root] = new PartialScreenshotLayer(
        Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer());
  }

  cursor_setter_.reset(new ScopedCursorSetter(
      Shell::GetInstance()->cursor_manager(), ui::kCursorCross));

  EnableMouseWarp(false);
}

void PartialScreenshotController::MaybeStart(const ui::LocatedEvent& event) {
  aura::Window* current_root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  if (root_window_) {
    // It's already started. This can happen when the second finger touches
    // the screen, or combination of the touch and mouse. We should grab the
    // partial screenshot instead of restarting.
    if (current_root == root_window_) {
      Update(event);
      Complete();
    }
  } else {
    root_window_ = current_root;
    start_position_ = event.root_location();
  }
}

void PartialScreenshotController::Complete() {
  if (!root_window_) {
    // If we received a released event before we ever got a pressed event
    // (resulting in setting |root_window_|), we just return without canceling
    // to keep the screenshot session active waiting for the next press.
    //
    // This is to avoid a crash that used to happen when we start the screenshot
    // session while the mouse is pressed and then release without moving the
    // mouse. crbug.com/581432.
    return;
  }

  DCHECK(layers_.count(root_window_));
  const gfx::Rect& region = layers_.at(root_window_)->region();
  if (!region.IsEmpty()) {
    screenshot_delegate_->HandleTakePartialScreenshot(
        root_window_, gfx::IntersectRects(root_window_->bounds(), region));
  }
  Cancel();
}

void PartialScreenshotController::Cancel() {
  root_window_ = nullptr;
  screenshot_delegate_ = nullptr;
  gfx::Screen::GetScreen()->RemoveObserver(this);
  STLDeleteValues(&layers_);
  cursor_setter_.reset();
  EnableMouseWarp(true);
}

void PartialScreenshotController::Update(const ui::LocatedEvent& event) {
  // Update may happen without MaybeStart() if the partial screenshot session
  // starts when dragging.
  if (!root_window_)
    MaybeStart(event);

  DCHECK(layers_.find(root_window_) != layers_.end());
  layers_.at(root_window_)
      ->SetRegion(
          gfx::Rect(std::min(start_position_.x(), event.root_location().x()),
                    std::min(start_position_.y(), event.root_location().y()),
                    ::abs(start_position_.x() - event.root_location().x()),
                    ::abs(start_position_.y() - event.root_location().y())));
}

void PartialScreenshotController::OnKeyEvent(ui::KeyEvent* event) {
  if (!screenshot_delegate_)
    return;
  if (event->type() == ui::ET_KEY_RELEASED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    Cancel();
  }

  // Intercepts all key events.
  event->StopPropagation();
}

void PartialScreenshotController::OnMouseEvent(ui::MouseEvent* event) {
  if (!screenshot_delegate_)
    return;
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      MaybeStart(*event);
      break;
    case ui::ET_MOUSE_DRAGGED:
      Update(*event);
      break;
    case ui::ET_MOUSE_RELEASED:
      Complete();
      break;
    default:
      // Do nothing.
      break;
  }
  event->StopPropagation();
}

void PartialScreenshotController::OnTouchEvent(ui::TouchEvent* event) {
  if (!screenshot_delegate_)
    return;
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      MaybeStart(*event);
      break;
    case ui::ET_TOUCH_MOVED:
      Update(*event);
      break;
    case ui::ET_TOUCH_RELEASED:
      Complete();
      break;
    default:
      // Do nothing.
      break;
  }
  event->StopPropagation();
}

void PartialScreenshotController::OnDisplayAdded(
    const gfx::Display& new_display) {
  if (!screenshot_delegate_)
    return;
  Cancel();
}

void PartialScreenshotController::OnDisplayRemoved(
    const gfx::Display& old_display) {
  if (!screenshot_delegate_)
    return;
  Cancel();
}

void PartialScreenshotController::OnDisplayMetricsChanged(
    const gfx::Display& display,
    uint32_t changed_metrics) {
}

}  // namespace ash
