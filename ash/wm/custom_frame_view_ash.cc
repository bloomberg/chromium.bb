// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/ash_switches.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/caption_buttons/frame_maximize_button.h"
#include "ash/wm/caption_buttons/frame_maximize_button_observer.h"
#include "ash/wm/frame_border_hit_test_controller.h"
#include "ash/wm/header_painter.h"
#include "ash/wm/immersive_fullscreen_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "grit/ash_resources.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"

namespace {

const gfx::FontList& GetTitleFontList() {
  static const gfx::FontList* title_font_list =
      new gfx::FontList(views::NativeWidgetAura::GetWindowTitleFontList());
  ANNOTATE_LEAKING_OBJECT_PTR(title_font_list);
  return *title_font_list;
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAshWindowStateDelegate

// Handles a user's fullscreen request (Shift+F4/F4). Puts the window into
// immersive fullscreen if immersive fullscreen is enabled for non-browser
// windows.
class CustomFrameViewAshWindowStateDelegate
    : public ash::wm::WindowStateDelegate,
      public ash::wm::WindowStateObserver,
      public aura::WindowObserver {
 public:
  CustomFrameViewAshWindowStateDelegate(
      ash::wm::WindowState* window_state,
      ash::CustomFrameViewAsh* custom_frame_view)
      : window_state_(NULL) {
#if defined(OS_CHROMEOS)
    // TODO(pkotwicz): Investigate if immersive fullscreen can be enabled for
    // Windows Ash.
    if (ash::switches::UseImmersiveFullscreenForAllWindows()) {
      immersive_fullscreen_controller_.reset(
          new ash::ImmersiveFullscreenController);
      custom_frame_view->InitImmersiveFullscreenControllerForView(
          immersive_fullscreen_controller_.get());

      // Add a window state observer to exit fullscreen properly in case
      // fullscreen is exited without going through
      // WindowState::ToggleFullscreen(). This is the case when exiting
      // immersive fullscreen via the "Restore" window control.
      // TODO(pkotwicz): This is a hack. Remove ASAP. http://crbug.com/319048
      window_state_ = window_state;
      window_state_->AddObserver(this);
      window_state_->window()->AddObserver(this);
    }
#endif
  }
  virtual ~CustomFrameViewAshWindowStateDelegate() {
    if (window_state_) {
      window_state_->RemoveObserver(this);
      window_state_->window()->RemoveObserver(this);
    }
  }
 private:
  // Overridden from ash::wm::WindowStateDelegate:
  virtual bool ToggleFullscreen(ash::wm::WindowState* window_state) OVERRIDE {
    bool enter_fullscreen = !window_state->IsFullscreen();
    if (enter_fullscreen) {
      window_state->window()->SetProperty(aura::client::kShowStateKey,
                                           ui::SHOW_STATE_FULLSCREEN);
    } else {
      window_state->Restore();
    }
    if (immersive_fullscreen_controller_) {
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_OTHER,
          enter_fullscreen);
    }
    return true;
  }
  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    window_state_->RemoveObserver(this);
    window_state_->window()->RemoveObserver(this);
    window_state_ = NULL;
  }
  // Overridden from ash::wm::WindowStateObserver:
  virtual void OnPostWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::wm::WindowStateType old_type) OVERRIDE {
    if (!window_state->IsFullscreen() &&
        !window_state->IsMinimized() &&
        immersive_fullscreen_controller_.get() &&
        immersive_fullscreen_controller_->IsEnabled()) {
      immersive_fullscreen_controller_->SetEnabled(
          ash::ImmersiveFullscreenController::WINDOW_TYPE_OTHER,
          false);
    }
  }

  ash::wm::WindowState* window_state_;
  scoped_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshWindowStateDelegate);
};

}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::HeaderView

// View which paints the header. It slides off and on screen in immersive
// fullscreen.
class CustomFrameViewAsh::HeaderView
    : public views::View,
      public ImmersiveFullscreenController::Delegate,
      public FrameMaximizeButtonObserver {
 public:
  // |frame| is the widget that the caption buttons act on.
  explicit HeaderView(views::Widget* frame);
  virtual ~HeaderView();

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the amount of the view's pixels which should be on screen.
  int GetPreferredOnScreenHeight() const;

  // Returns the view's preferred height.
  int GetPreferredHeight() const;

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  HeaderPainter* header_painter() {
    return header_painter_.get();
  }

 private:
  // ImmersiveFullscreenController::Delegate overrides:
  virtual void OnImmersiveRevealStarted() OVERRIDE;
  virtual void OnImmersiveRevealEnded() OVERRIDE;
  virtual void OnImmersiveFullscreenExited() OVERRIDE;
  virtual void SetVisibleFraction(double visible_fraction) OVERRIDE;
  virtual std::vector<gfx::Rect> GetVisibleBoundsInScreen() const OVERRIDE;

  // FrameMaximizeButtonObserver overrides:
  virtual void OnMaximizeBubbleShown(views::Widget* bubble) OVERRIDE;

  // The widget that the caption buttons act on.
  views::Widget* frame_;

  // Helper for painting the header.
  scoped_ptr<HeaderPainter> header_painter_;

  // View which contains the window caption buttons.
  FrameCaptionButtonContainerView* caption_button_container_;

  // The maximize bubble widget. |maximize_bubble_| may be non-NULL but have
  // been already destroyed.
  views::Widget* maximize_bubble_;

  // Keeps track of whether |maximize_bubble_| is still alive.
  scoped_ptr<views::WidgetDeletionObserver> maximize_bubble_lifetime_observer_;

  // The fraction of the header's height which is visible while in fullscreen.
  // This value is meaningless when not in fullscreen.
  double fullscreen_visible_fraction_;

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

CustomFrameViewAsh::HeaderView::HeaderView(views::Widget* frame)
    : frame_(frame),
      header_painter_(new ash::HeaderPainter),
      caption_button_container_(NULL),
      maximize_bubble_(NULL),
      fullscreen_visible_fraction_(0) {
  // Unfortunately, there is no views::WidgetDelegate::CanMinimize(). Assume
  // that the window frame can be minimized if it can be maximized.
  FrameCaptionButtonContainerView::MinimizeAllowed minimize_allowed =
      frame_->widget_delegate()->CanMaximize() ?
          FrameCaptionButtonContainerView::MINIMIZE_ALLOWED :
          FrameCaptionButtonContainerView::MINIMIZE_DISALLOWED;
  caption_button_container_ = new FrameCaptionButtonContainerView(frame_,
      minimize_allowed);
  AddChildView(caption_button_container_);
  FrameMaximizeButton* frame_maximize_button =
      caption_button_container_->GetOldStyleSizeButton();
  if (frame_maximize_button)
    frame_maximize_button->AddObserver(this);

  header_painter_->Init(HeaderPainter::STYLE_OTHER, frame_, this, NULL,
      caption_button_container_);
}

CustomFrameViewAsh::HeaderView::~HeaderView() {
  FrameMaximizeButton* frame_maximize_button =
      caption_button_container_->GetOldStyleSizeButton();
  if (frame_maximize_button)
    frame_maximize_button->RemoveObserver(this);
}

void CustomFrameViewAsh::HeaderView::SchedulePaintForTitle() {
  header_painter_->SchedulePaintForTitle(GetTitleFontList());
}

void CustomFrameViewAsh::HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int CustomFrameViewAsh::HeaderView::GetPreferredOnScreenHeight() const {
  if (frame_->IsFullscreen()) {
    return static_cast<int>(
        GetPreferredHeight() * fullscreen_visible_fraction_);
  }
  return GetPreferredHeight();
}

int CustomFrameViewAsh::HeaderView::GetPreferredHeight() const {
  // Reserve enough space to see the buttons and the separator line.
  return caption_button_container_->bounds().bottom() +
      header_painter_->HeaderContentSeparatorSize();
}

int CustomFrameViewAsh::HeaderView::GetMinimumWidth() const {
  return header_painter_->GetMinimumHeaderWidth();
}

void CustomFrameViewAsh::HeaderView::Layout() {
  header_painter_->LayoutHeader();
  header_painter_->set_header_height(GetPreferredHeight());
}

void CustomFrameViewAsh::HeaderView::OnPaint(gfx::Canvas* canvas) {
  bool paint_as_active =
      frame_->non_client_view()->frame_view()->ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(paint_as_active);

  int theme_image_id = 0;
  if (paint_as_active)
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  else
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;

  HeaderPainter::Mode header_mode = paint_as_active ?
      HeaderPainter::MODE_ACTIVE : HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(
      canvas,
      header_mode,
      theme_image_id,
      0);
  header_painter_->PaintTitleBar(canvas, GetTitleFontList());
  header_painter_->PaintHeaderContentSeparator(canvas, header_mode);
}

void CustomFrameViewAsh::HeaderView::OnImmersiveRevealStarted() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::OnImmersiveRevealEnded() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::OnImmersiveFullscreenExited() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::SetVisibleFraction(
    double visible_fraction) {
  if (fullscreen_visible_fraction_ != visible_fraction) {
    fullscreen_visible_fraction_ = visible_fraction;
    parent()->Layout();
  }
}

std::vector<gfx::Rect>
CustomFrameViewAsh::HeaderView::GetVisibleBoundsInScreen() const {
  // TODO(pkotwicz): Implement views::View::ConvertRectToScreen().
  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin_in_screen(visible_bounds.origin());
  views::View::ConvertPointToScreen(this, &visible_origin_in_screen);
  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(
      gfx::Rect(visible_origin_in_screen, visible_bounds.size()));
  if (maximize_bubble_lifetime_observer_.get() &&
      maximize_bubble_lifetime_observer_->IsWidgetAlive()) {
    bounds_in_screen.push_back(maximize_bubble_->GetWindowBoundsInScreen());
  }
  return bounds_in_screen;
}

void CustomFrameViewAsh::HeaderView::OnMaximizeBubbleShown(
    views::Widget* bubble) {
  maximize_bubble_ = bubble;
  maximize_bubble_lifetime_observer_.reset(
      new views::WidgetDeletionObserver(bubble));
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::OverlayView

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class CustomFrameViewAsh::OverlayView : public views::View {
 public:
  explicit OverlayView(HeaderView* header_view);
  virtual ~OverlayView();

  // views::View override:
  virtual void Layout() OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;

 private:
  HeaderView* header_view_;

  DISALLOW_COPY_AND_ASSIGN(OverlayView);
};

CustomFrameViewAsh::OverlayView::OverlayView(HeaderView* header_view)
    : header_view_(header_view) {
  AddChildView(header_view);
}

CustomFrameViewAsh::OverlayView::~OverlayView() {
}

void CustomFrameViewAsh::OverlayView::Layout() {
  // Layout |header_view_| because layout affects the result of
  // GetPreferredOnScreenHeight().
  header_view_->Layout();

  int onscreen_height = header_view_->GetPreferredOnScreenHeight();
  if (onscreen_height == 0) {
    header_view_->SetVisible(false);
  } else {
    int height = header_view_->GetPreferredHeight();
    header_view_->SetBounds(0, onscreen_height - height, width(), height);
    header_view_->SetVisible(true);
  }
}

bool CustomFrameViewAsh::OverlayView::HitTestRect(const gfx::Rect& rect) const {
  // Grab events in the header view. Return false for other events so that they
  // can be handled by the client view.
  return header_view_->HitTestRect(rect);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, public:

// static
const char CustomFrameViewAsh::kViewClassName[] = "CustomFrameViewAsh";

CustomFrameViewAsh::CustomFrameViewAsh(views::Widget* frame)
    : frame_(frame),
      header_view_(new HeaderView(frame)),
      frame_border_hit_test_controller_(
          new FrameBorderHitTestController(frame_)) {
  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  frame->non_client_view()->SetOverlayView(new OverlayView(header_view_));

  // A delegate for a more complex way of fullscreening the window may already
  // be set. This is the case for packaged apps.
  wm::WindowState* window_state = wm::GetWindowState(frame->GetNativeWindow());
  if (!window_state->HasDelegate()) {
    window_state->SetDelegate(scoped_ptr<wm::WindowStateDelegate>(
        new CustomFrameViewAshWindowStateDelegate(
            window_state, this)));
  }
}

CustomFrameViewAsh::~CustomFrameViewAsh() {
}

void CustomFrameViewAsh::InitImmersiveFullscreenControllerForView(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  immersive_fullscreen_controller->Init(header_view_, frame_, header_view_);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::NonClientFrameView overrides:

gfx::Rect CustomFrameViewAsh::GetBoundsForClientView() const {
  int top_height = NonClientTopBorderHeight();
  return HeaderPainter::GetBoundsForClientView(top_height, bounds());
}

gfx::Rect CustomFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  return HeaderPainter::GetWindowBoundsForClientBounds(top_height,
                                                       client_bounds);
}

int CustomFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderHitTestController::NonClientHitTest(this,
      header_view_->header_painter(), point);
}

void CustomFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                       gfx::Path* window_mask) {
  // No window masks in Aura.
}

void CustomFrameViewAsh::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void CustomFrameViewAsh::UpdateWindowIcon() {
}

void CustomFrameViewAsh::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::View overrides:

gfx::Size CustomFrameViewAsh::GetPreferredSize() {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      bounds).size();
}

const char* CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size CustomFrameViewAsh::GetMinimumSize() {
  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size CustomFrameViewAsh::GetMaximumSize() {
  gfx::Size max_client_size(frame_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0)
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  if (max_client_size.height() > 0)
    height = NonClientTopBorderHeight() + max_client_size.height();

  return gfx::Size(width, height);
}

void CustomFrameViewAsh::SchedulePaintInRect(const gfx::Rect& r) {
  // We may end up here before |header_view_| has been added to the Widget.
  if (header_view_->GetWidget()) {
    // The HeaderView is not a child of CustomFrameViewAsh. Redirect the paint
    // to HeaderView instead.
    gfx::RectF to_paint(r);
    views::View::ConvertRectToTarget(this, header_view_, &to_paint);
    header_view_->SchedulePaintInRect(gfx::ToEnclosingRect(to_paint));
  } else {
    views::NonClientFrameView::SchedulePaintInRect(r);
  }
}

bool CustomFrameViewAsh::HitTestRect(const gfx::Rect& rect) const {
  // NonClientView hit tests the NonClientFrameView first instead of going in
  // z-order. Return false so that events get to the OverlayView.
  return false;
}

views::View* CustomFrameViewAsh::GetHeaderView() {
  return header_view_;
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  return frame_->IsFullscreen() ? 0 : header_view_->GetPreferredHeight();
}

}  // namespace ash
