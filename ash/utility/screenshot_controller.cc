// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/screenshot_controller.h"

#include <cmath>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
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

class ScreenshotWindowTargeter : public aura::WindowTargeter {
 public:
  ScreenshotWindowTargeter() = default;
  ~ScreenshotWindowTargeter() override = default;

  aura::Window* FindWindowForEvent(ui::LocatedEvent* event) {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    aura::Window* target_root = target->GetRootWindow();

    aura::client::ScreenPositionClient* position_client =
        aura::client::GetScreenPositionClient(target_root);
    gfx::Point location = event->location();
    position_client->ConvertPointToScreen(target, &location);

    gfx::Display display =
        gfx::Screen::GetScreen()->GetDisplayNearestPoint(location);

    aura::Window* root_window = Shell::GetInstance()
                                    ->window_tree_host_manager()
                                    ->GetRootWindowForDisplayId(display.id());

    position_client->ConvertPointFromScreen(root_window, &location);

    gfx::Point target_location = event->location();
    event->set_location(location);

    // Ignore capture window when finding the target for located event.
    aura::client::CaptureClient* original_capture_client =
        aura::client::GetCaptureClient(root_window);
    aura::client::SetCaptureClient(root_window, nullptr);

    aura::Window* selected =
        static_cast<aura::Window*>(FindTargetForEvent(root_window, event));

    // Restore State.
    aura::client::SetCaptureClient(root_window, original_capture_client);
    event->set_location(target_location);
    return selected;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotWindowTargeter);
};

}  // namespace

class ScreenshotController::ScreenshotLayer : public ui::LayerOwner,
                                              public ui::LayerDelegate {
 public:
  ScreenshotLayer(ui::Layer* parent) {
    SetLayer(new ui::Layer(ui::LAYER_TEXTURED));
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBounds(parent->bounds());
    parent->Add(layer());
    parent->StackAtTop(layer());
    layer()->SetVisible(true);
    layer()->set_delegate(this);
  }
  ~ScreenshotLayer() override {}

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
    const SkColor kSelectedAreaOverlayColor = 0x40000000;
    if (region_.IsEmpty())
      return;
    // Screenshot area representation: black rectangle with white
    // rectangle inside.  To avoid capturing these rectangles when mouse
    // release, they should be outside of the actual capturing area.
    gfx::Rect rect(region_);
    ui::PaintRecorder recorder(context, layer()->size());

    recorder.canvas()->FillRect(region_, kSelectedAreaOverlayColor);

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

  DISALLOW_COPY_AND_ASSIGN(ScreenshotLayer);
};

class ScreenshotController::ScopedCursorSetter {
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

ScreenshotController::ScreenshotController()
    : mode_(NONE),
      root_window_(nullptr),
      selected_(nullptr),
      screenshot_delegate_(nullptr) {
  // Keep this here and don't move it to StartPartialScreenshotSession(), as it
  // needs to be pre-pended by MouseCursorEventFilter in Shell::Init().
  Shell::GetInstance()->PrependPreTargetHandler(this);
}

ScreenshotController::~ScreenshotController() {
  if (screenshot_delegate_)
    Cancel();
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void ScreenshotController::StartWindowScreenshotSession(
    ScreenshotDelegate* screenshot_delegate) {
  if (screenshot_delegate_) {
    DCHECK_EQ(screenshot_delegate_, screenshot_delegate);
    return;
  }
  screenshot_delegate_ = screenshot_delegate;
  mode_ = WINDOW;

  gfx::Screen::GetScreen()->AddObserver(this);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    layers_[root] = new ScreenshotLayer(
        Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer());
  }
  SetSelectedWindow(wm::GetActiveWindow());

  cursor_setter_.reset(new ScopedCursorSetter(
      Shell::GetInstance()->cursor_manager(), ui::kCursorCross));

  EnableMouseWarp(true);
}

void ScreenshotController::StartPartialScreenshotSession(
    ScreenshotDelegate* screenshot_delegate) {
  // Already in a screenshot session.
  if (screenshot_delegate_) {
    DCHECK_EQ(screenshot_delegate_, screenshot_delegate);
    return;
  }

  screenshot_delegate_ = screenshot_delegate;
  mode_ = PARTIAL;
  gfx::Screen::GetScreen()->AddObserver(this);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    layers_[root] = new ScreenshotLayer(
        Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer());
  }

  cursor_setter_.reset(new ScopedCursorSetter(
      Shell::GetInstance()->cursor_manager(), ui::kCursorCross));

  EnableMouseWarp(false);
}

void ScreenshotController::MaybeStart(const ui::LocatedEvent& event) {
  aura::Window* current_root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  if (root_window_) {
    // It's already started. This can happen when the second finger touches
    // the screen, or combination of the touch and mouse. We should grab the
    // partial screenshot instead of restarting.
    if (current_root == root_window_) {
      Update(event);
      CompletePartialScreenshot();
    }
  } else {
    root_window_ = current_root;
    start_position_ = event.root_location();
  }
}

void ScreenshotController::CompleteWindowScreenshot() {
  if (selected_)
    screenshot_delegate_->HandleTakeWindowScreenshot(selected_);
  Cancel();
}

void ScreenshotController::CompletePartialScreenshot() {
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

void ScreenshotController::Cancel() {
  mode_ = NONE;
  root_window_ = nullptr;
  SetSelectedWindow(nullptr);
  screenshot_delegate_ = nullptr;
  gfx::Screen::GetScreen()->RemoveObserver(this);
  STLDeleteValues(&layers_);
  cursor_setter_.reset();
  EnableMouseWarp(true);
}

void ScreenshotController::Update(const ui::LocatedEvent& event) {
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

void ScreenshotController::UpdateSelectedWindow(ui::LocatedEvent* event) {
  aura::Window* selected = ScreenshotWindowTargeter().FindWindowForEvent(event);

  // Find a window that is backed with a widget.
  while (selected && (selected->type() == ui::wm::WINDOW_TYPE_CONTROL ||
                      !selected->delegate())) {
    selected = selected->parent();
  }

  if (selected->parent()->id() == kShellWindowId_DesktopBackgroundContainer ||
      selected->parent()->id() == kShellWindowId_LockScreenBackgroundContainer)
    selected = nullptr;

  SetSelectedWindow(selected);
}

void ScreenshotController::SetSelectedWindow(aura::Window* selected) {
  if (selected_ == selected)
    return;

  if (selected_) {
    selected_->RemoveObserver(this);
    layers_.at(selected_->GetRootWindow())->SetRegion(gfx::Rect());
  }

  selected_ = selected;

  if (selected_) {
    selected_->AddObserver(this);
    layers_.at(selected_->GetRootWindow())->SetRegion(selected_->bounds());
  }
}

void ScreenshotController::OnKeyEvent(ui::KeyEvent* event) {
  if (!screenshot_delegate_)
    return;

  if (event->type() == ui::ET_KEY_RELEASED) {
    if (event->key_code() == ui::VKEY_ESCAPE) {
      Cancel();
    } else if (event->key_code() == ui::VKEY_RETURN && mode_ == WINDOW) {
      CompleteWindowScreenshot();
    }
  }

  // Intercepts all key events.
  event->StopPropagation();
}

void ScreenshotController::OnMouseEvent(ui::MouseEvent* event) {
  if (!screenshot_delegate_)
    return;
  switch (mode_) {
    case NONE:
      NOTREACHED();
      break;
    case WINDOW:
      switch (event->type()) {
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
          UpdateSelectedWindow(event);
          break;
        case ui::ET_MOUSE_RELEASED:
          CompleteWindowScreenshot();
          break;
        default:
          // Do nothing.
          break;
      }
      break;
    case PARTIAL:
      switch (event->type()) {
        case ui::ET_MOUSE_PRESSED:
          MaybeStart(*event);
          break;
        case ui::ET_MOUSE_DRAGGED:
          Update(*event);
          break;
        case ui::ET_MOUSE_RELEASED:
          CompletePartialScreenshot();
          break;
        default:
          // Do nothing.
          break;
      }
      break;
  }
  event->StopPropagation();
}

void ScreenshotController::OnTouchEvent(ui::TouchEvent* event) {
  if (!screenshot_delegate_)
    return;
  switch (mode_) {
    case NONE:
      NOTREACHED();
      break;
    case WINDOW:
      switch (event->type()) {
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_MOVED:
          UpdateSelectedWindow(event);
          break;
        case ui::ET_TOUCH_RELEASED:
          CompleteWindowScreenshot();
          break;
        default:
          // Do nothing.
          break;
      }
      break;
    case PARTIAL:
      switch (event->type()) {
        case ui::ET_TOUCH_PRESSED:
          MaybeStart(*event);
          break;
        case ui::ET_TOUCH_MOVED:
          Update(*event);
          break;
        case ui::ET_TOUCH_RELEASED:
          CompletePartialScreenshot();
          break;
        default:
          // Do nothing.
          break;
      }
      break;
  }
  event->StopPropagation();
}

void ScreenshotController::OnDisplayAdded(const gfx::Display& new_display) {
  if (!screenshot_delegate_)
    return;
  Cancel();
}

void ScreenshotController::OnDisplayRemoved(const gfx::Display& old_display) {
  if (!screenshot_delegate_)
    return;
  Cancel();
}

void ScreenshotController::OnDisplayMetricsChanged(const gfx::Display& display,
                                                   uint32_t changed_metrics) {}

void ScreenshotController::OnWindowDestroying(aura::Window* window) {
  SetSelectedWindow(nullptr);
}

}  // namespace ash
