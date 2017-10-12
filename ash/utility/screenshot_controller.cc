// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/screenshot_controller.h"

#include <cmath>
#include <memory>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

const int kCursorSize = 12;

// This will prevent the user from taking a screenshot across multiple
// monitors. it will stop the mouse at the any edge of the screen. must
// swtich back on when the screenshot is complete.
void EnableMouseWarp(bool enable) {
  Shell::Get()->mouse_cursor_filter()->set_mouse_warp_enabled(enable);
}

// Returns the target for the specified event ignorning any capture windows.
aura::Window* FindWindowForEvent(const ui::LocatedEvent& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  aura::Window* target_root = target->GetRootWindow();

  aura::client::ScreenPositionClient* position_client =
      aura::client::GetScreenPositionClient(target_root);
  gfx::Point location = event.location();
  position_client->ConvertPointToScreen(target, &location);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(location);

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());

  position_client->ConvertPointFromScreen(root_window, &location);

  std::unique_ptr<ui::Event> cloned_event = ui::Event::Clone(event);
  ui::LocatedEvent* cloned_located_event = cloned_event->AsLocatedEvent();
  cloned_located_event->set_location(location);

  // Ignore capture window when finding the target for located event.
  aura::client::CaptureClient* original_capture_client =
      aura::client::GetCaptureClient(root_window);
  aura::client::SetCaptureClient(root_window, nullptr);

  aura::Window* selected =
      static_cast<aura::Window*>(aura::WindowTargeter().FindTargetForEvent(
          root_window, cloned_located_event));

  // Restore State.
  aura::client::SetCaptureClient(root_window, original_capture_client);
  return selected;
}

}  // namespace

class ScreenshotController::ScreenshotLayer : public ui::LayerOwner,
                                              public ui::LayerDelegate {
 public:
  ScreenshotLayer(ui::Layer* parent, bool immediate_overlay)
      : draw_inactive_overlay_(immediate_overlay) {
    SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
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
    union_rect.Intersects(layer()->bounds());
    union_rect.Inset(-kCursorSize, -kCursorSize, -kCursorSize, -kCursorSize);
    region_ = region;
    layer()->SchedulePaint(union_rect);

    // If we are going to start drawing the inactive overlay, we need to
    // invalidate the entire layer.
    bool is_drawing_inactive_overlay = draw_inactive_overlay_;
    draw_inactive_overlay_ = draw_inactive_overlay_ || !region.IsEmpty();
    if (draw_inactive_overlay_ && !is_drawing_inactive_overlay)
      layer()->SchedulePaint(layer()->parent()->bounds());
  }

  void set_cursor_location_in_root(const gfx::Point& point) {
    cursor_location_in_root_ = point;
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const SkColor kSelectedAreaOverlayColor = 0x60000000;
    // Screenshot area representation: transparent hole with half opaque gray
    // overlay.
    ui::PaintRecorder recorder(context, layer()->size());

    if (draw_inactive_overlay_) {
      recorder.canvas()->FillRect(gfx::Rect(layer()->size()),
                                  kSelectedAreaOverlayColor);
    }

    DrawPseudoCursor(recorder.canvas());

    if (!region_.IsEmpty())
      recorder.canvas()->FillRect(region_, SK_ColorBLACK, SkBlendMode::kClear);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // Mouse cursor may move sub DIP, so paint pseudo cursor instead of
  // using platform cursor so that it's aliend with the region.
  void DrawPseudoCursor(gfx::Canvas* canvas) {
    // Don't draw if window selection mode.
    if (cursor_location_in_root_.IsOrigin())
      return;

    gfx::Point pseudo_cursor_point = cursor_location_in_root_;

    // The cursor is above/before region.
    if (pseudo_cursor_point.x() == region_.x())
      pseudo_cursor_point.Offset(-1, 0);

    if (pseudo_cursor_point.y() == region_.y())
      pseudo_cursor_point.Offset(0, -1);

    cc::PaintFlags flags;
    flags.setAntiAlias(false);
    flags.setStrokeWidth(1);
    flags.setColor(SK_ColorWHITE);
    flags.setBlendMode(SkBlendMode::kSrc);
    gfx::Vector2d width(kCursorSize / 2, 0);
    gfx::Vector2d height(0, kCursorSize / 2);
    gfx::Vector2d white_x_offset(1, -1);
    gfx::Vector2d white_y_offset(1, -1);
    // Horizontal
    canvas->DrawLine(pseudo_cursor_point - width + white_x_offset,
                     pseudo_cursor_point + width + white_x_offset, flags);
    flags.setStrokeWidth(1);
    // Vertical
    canvas->DrawLine(pseudo_cursor_point - height + white_y_offset,
                     pseudo_cursor_point + height + white_y_offset, flags);

    flags.setColor(SK_ColorBLACK);
    // Horizontal
    canvas->DrawLine(pseudo_cursor_point - width, pseudo_cursor_point + width,
                     flags);
    // Vertical
    canvas->DrawLine(pseudo_cursor_point - height, pseudo_cursor_point + height,
                     flags);
  }

  bool draw_inactive_overlay_;

  gfx::Rect region_;

  gfx::Point cursor_location_in_root_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotLayer);
};

class ScreenshotController::ScopedCursorSetter {
 public:
  ScopedCursorSetter(::wm::CursorManager* cursor_manager,
                     ui::CursorType cursor) {
    if (cursor_manager)
      InitializeWithCursorManager(cursor_manager, cursor);
    else
      InitializeWithShellPort(cursor);
  }

  ~ScopedCursorSetter() {
    if (cursor_manager_) {
      cursor_manager_->UnlockCursor();
    } else if (already_locked_) {
      // No action; we didn't lock the cursor because it was already locked.
    } else {
      ShellPort::Get()->UnlockCursor();
    }
  }

 private:
  void InitializeWithCursorManager(::wm::CursorManager* cursor_manager,
                                   ui::CursorType cursor) {
    if (cursor_manager->IsCursorLocked()) {
      already_locked_ = true;
      return;
    }
    gfx::NativeCursor original_cursor = cursor_manager->GetCursor();
    cursor_manager_ = cursor_manager;
    if (cursor == ui::CursorType::kNone) {
      cursor_manager_->HideCursor();
    } else {
      cursor_manager_->SetCursor(cursor);
      cursor_manager_->ShowCursor();
    }
    cursor_manager_->LockCursor();
    // Set/ShowCursor does not make any effects at this point but it sets
    // back to the original cursor when unlocked.
    cursor_manager_->SetCursor(original_cursor);
    cursor_manager_->ShowCursor();
  }

  void InitializeWithShellPort(ui::CursorType cursor) {
    // No cursor manager. We are in mus mode.
    ShellPort* port = ShellPort::Get();
    if (cursor == ui::CursorType::kNone) {
      port->HideCursor();
    } else {
      port->SetGlobalOverrideCursor(ui::CursorData(cursor));
      port->ShowCursor();
    }
    port->LockCursor();

    // Set/ShowCursor does not make any effects at this point but it sets
    // back to the original cursor when unlocked.
    port->SetGlobalOverrideCursor(base::nullopt);
    port->ShowCursor();
  }

  // If the cursor is already locked, don't try to lock it again.
  bool already_locked_ = false;

  // If we were given a valid CursorManager, and the cursor wasn't locked, keep
  // track of the CursorManager we sent a LockCursor() call to so we can unlock
  // it in the destructor.
  ::wm::CursorManager* cursor_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorSetter);
};

ScreenshotController::ScreenshotController()
    : mode_(NONE),
      root_window_(nullptr),
      selected_(nullptr),
      screenshot_delegate_(nullptr) {
  // Keep this here and don't move it to StartPartialScreenshotSession(), as it
  // needs to be pre-pended by MouseCursorEventFilter in Shell::Init().
  Shell::Get()->PrependPreTargetHandler(this);
}

ScreenshotController::~ScreenshotController() {
  if (screenshot_delegate_)
    CancelScreenshotSession();
  Shell::Get()->RemovePreTargetHandler(this);
}

void ScreenshotController::StartWindowScreenshotSession(
    ScreenshotDelegate* screenshot_delegate) {
  if (screenshot_delegate_) {
    DCHECK_EQ(screenshot_delegate_, screenshot_delegate);
    return;
  }
  screenshot_delegate_ = screenshot_delegate;
  mode_ = WINDOW;

  display::Screen::GetScreen()->AddObserver(this);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    layers_[root] = std::make_unique<ScreenshotLayer>(
        Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer(),
        true);
  }
  SetSelectedWindow(wm::GetActiveWindow());

  cursor_setter_.reset(new ScopedCursorSetter(Shell::Get()->cursor_manager(),
                                              ui::CursorType::kCross));

  EnableMouseWarp(true);
}

void ScreenshotController::StartPartialScreenshotSession(
    ScreenshotDelegate* screenshot_delegate,
    bool draw_overlay_immediately) {
  // Already in a screenshot session.
  if (screenshot_delegate_) {
    DCHECK_EQ(screenshot_delegate_, screenshot_delegate);
    return;
  }

  screenshot_delegate_ = screenshot_delegate;
  mode_ = PARTIAL;
  display::Screen::GetScreen()->AddObserver(this);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    layers_[root] = std::make_unique<ScreenshotLayer>(
        Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer(),
        draw_overlay_immediately);
  }

  if (!pen_events_only_) {
    cursor_setter_.reset(new ScopedCursorSetter(Shell::Get()->cursor_manager(),
                                                ui::CursorType::kCross));
  }

  EnableMouseWarp(false);
}

void ScreenshotController::CancelScreenshotSession() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    // Having pre-handled all mouse events, widgets that had mouse capture may
    // now misbehave, so break any existing captures. Do this after the
    // screenshot session is over so that it's still possible to screenshot
    // things like menus.
    aura::client::GetCaptureClient(root)->SetCapture(nullptr);
  }

  mode_ = NONE;
  pen_events_only_ = false;
  root_window_ = nullptr;
  SetSelectedWindow(nullptr);
  screenshot_delegate_ = nullptr;
  display::Screen::GetScreen()->RemoveObserver(this);
  layers_.clear();
  cursor_setter_.reset();
  EnableMouseWarp(true);

  if (on_screenshot_session_done_)
    std::move(on_screenshot_session_done_).Run();
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
    if (!pen_events_only_) {
      // ScopedCursorSetter must be reset first to make sure that its dtor is
      // called before ctor is called.
      cursor_setter_.reset();
      cursor_setter_.reset(new ScopedCursorSetter(
          Shell::Get()->cursor_manager(), ui::CursorType::kNone));
    }
    Update(event);
  }
}

void ScreenshotController::CompleteWindowScreenshot() {
  if (selected_)
    screenshot_delegate_->HandleTakeWindowScreenshot(selected_);
  CancelScreenshotSession();
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
  CancelScreenshotSession();
}

void ScreenshotController::Update(const ui::LocatedEvent& event) {
  // Update may happen without MaybeStart() if the partial screenshot session
  // starts when dragging.
  if (!root_window_)
    MaybeStart(event);
  DCHECK(layers_.find(root_window_) != layers_.end());

  ScreenshotLayer* layer = layers_.at(root_window_).get();
  layer->set_cursor_location_in_root(event.root_location());
  layer->SetRegion(
      gfx::Rect(std::min(start_position_.x(), event.root_location().x()),
                std::min(start_position_.y(), event.root_location().y()),
                ::abs(start_position_.x() - event.root_location().x()),
                ::abs(start_position_.y() - event.root_location().y())));
}

void ScreenshotController::UpdateSelectedWindow(const ui::LocatedEvent& event) {
  aura::Window* selected = FindWindowForEvent(event);

  // Find a window that is backed with a widget.
  while (selected && (selected->type() == aura::client::WINDOW_TYPE_CONTROL ||
                      !selected->delegate())) {
    selected = selected->parent();
  }

  if (selected->parent()->id() == kShellWindowId_WallpaperContainer ||
      selected->parent()->id() == kShellWindowId_LockScreenWallpaperContainer)
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

bool ScreenshotController::ShouldProcessEvent(
    const ui::PointerDetails& pointer_details) const {
  return !pen_events_only_ ||
         pointer_details.pointer_type == ui::EventPointerType::POINTER_TYPE_PEN;
}

void ScreenshotController::OnKeyEvent(ui::KeyEvent* event) {
  if (!screenshot_delegate_)
    return;

  if (event->type() == ui::ET_KEY_RELEASED) {
    if (event->key_code() == ui::VKEY_ESCAPE) {
      CancelScreenshotSession();
      event->StopPropagation();
    } else if (event->key_code() == ui::VKEY_RETURN && mode_ == WINDOW) {
      CompleteWindowScreenshot();
      event->StopPropagation();
    }
  }

  // Stop all key events except if the user is using a pointer, in which case
  // they should be able to continue manipulating the screen.
  if (!pen_events_only_)
    event->StopPropagation();
}

void ScreenshotController::OnMouseEvent(ui::MouseEvent* event) {
  if (!screenshot_delegate_ || !ShouldProcessEvent(event->pointer_details()))
    return;
  switch (mode_) {
    case NONE:
      NOTREACHED();
      break;
    case WINDOW:
      switch (event->type()) {
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
          UpdateSelectedWindow(*event);
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
  if (!screenshot_delegate_ || !ShouldProcessEvent(event->pointer_details()))
    return;
  switch (mode_) {
    case NONE:
      NOTREACHED();
      break;
    case WINDOW:
      switch (event->type()) {
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_MOVED:
          UpdateSelectedWindow(*event);
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

void ScreenshotController::OnDisplayAdded(const display::Display& new_display) {
  if (!screenshot_delegate_)
    return;
  CancelScreenshotSession();
}

void ScreenshotController::OnDisplayRemoved(
    const display::Display& old_display) {
  if (!screenshot_delegate_)
    return;
  CancelScreenshotSession();
}

void ScreenshotController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {}

void ScreenshotController::OnWindowDestroying(aura::Window* window) {
  SetSelectedWindow(nullptr);
}

}  // namespace ash
