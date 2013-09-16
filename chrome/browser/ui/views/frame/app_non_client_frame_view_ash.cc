// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_non_client_frame_view_ash.h"

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

// The size of the shadow around the caption buttons.
const int kShadowSizeX = 16;
const int kShadowSizeBottom = 21;

// The border for |control_view_|.
class ControlViewBorder : public views::Border {
 public:
  ControlViewBorder() {
    int border_id = base::i18n::IsRTL() ?
        IDR_AURA_WINDOW_FULLSCREEN_SHADOW_RTL :
        IDR_AURA_WINDOW_FULLSCREEN_SHADOW;
    border_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        border_id).AsImageSkia();
  }

  virtual ~ControlViewBorder() {
  }

 private:
  // views::Border overrides:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawImageInt(border_, 0, view.height() - border_.height());
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return gfx::Insets(0, kShadowSizeX, kShadowSizeBottom, 0);
  }

  gfx::ImageSkia border_;

  DISALLOW_COPY_AND_ASSIGN(ControlViewBorder);
};

// The background for |control_view_|.
class ControlViewBackground : public views::Background {
 public:
  explicit ControlViewBackground(bool is_incognito) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    int control_base_resource_id = is_incognito ?
        IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_ACTIVE :
        IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
    background_ = rb.GetImageNamed(control_base_resource_id).AsImageSkia();
  }

  virtual ~ControlViewBackground() {
  }

 private:
  // views::Background override:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    gfx::Rect paint_bounds(view->GetContentsBounds());
    paint_bounds.set_x(view->GetMirroredXForRect(paint_bounds));
    canvas->TileImageInt(background_, paint_bounds.x(), paint_bounds.y(),
        paint_bounds.width(), paint_bounds.height());
  }

  gfx::ImageSkia background_;

  DISALLOW_COPY_AND_ASSIGN(ControlViewBackground);
};

}  // namespace

// Observer to detect when the browser frame widget closes so we can clean
// up our ControlView. Because we can be closed via a keyboard shortcut we
// are not guaranteed to run AppNonClientFrameView's Close() or Restore().
class AppNonClientFrameViewAsh::FrameObserver : public views::WidgetObserver {
 public:
  explicit FrameObserver(AppNonClientFrameViewAsh* owner) : owner_(owner) {}
  virtual ~FrameObserver() {}

  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    owner_->CloseControlWidget();
  }

 private:
  AppNonClientFrameViewAsh* owner_;

  DISALLOW_COPY_AND_ASSIGN(FrameObserver);
};

// static
const char AppNonClientFrameViewAsh::kViewClassName[] =
    "AppNonClientFrameViewAsh";
// static
const char AppNonClientFrameViewAsh::kControlWindowName[] =
    "AppNonClientFrameViewAshControls";

AppNonClientFrameViewAsh::AppNonClientFrameViewAsh(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      control_view_(NULL),
      control_widget_(NULL),
      frame_observer_(new FrameObserver(this)) {
  control_view_ = new ash::FrameCaptionButtonContainerView(frame,
      ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  control_view_->set_header_style(
      ash::FrameCaptionButtonContainerView::HEADER_STYLE_MAXIMIZED_HOSTED_APP);
  control_view_->set_border(new ControlViewBorder());
  control_view_->set_background(new ControlViewBackground(
      browser_view->IsOffTheRecord()));

  // This FrameView is always maximized so we don't want the window to have
  // resize borders.
  frame->GetNativeView()->set_hit_test_bounds_override_inner(gfx::Insets());
  // Watch for frame close so we can clean up the control widget.
  frame->AddObserver(frame_observer_.get());
  set_background(views::Background::CreateSolidBackground(SK_ColorBLACK));
  // Create the controls.
  control_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.parent = browser_view->GetNativeWindow();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  control_widget_->Init(params);
  control_widget_->SetContentsView(control_view_);
  aura::Window* window = control_widget_->GetNativeView();
  window->SetName(kControlWindowName);

  // Need to exclude the shadow from the active control area.
  gfx::Insets hit_test_insets(control_view_->GetInsets());
  if (base::i18n::IsRTL()) {
    hit_test_insets = gfx::Insets(hit_test_insets.top(),
                                  hit_test_insets.right(),
                                  hit_test_insets.bottom(),
                                  hit_test_insets.left());
  }
  window->SetHitTestBoundsOverrideOuter(hit_test_insets, hit_test_insets);

  gfx::Rect control_bounds = GetControlBounds();
  window->SetBounds(control_bounds);
  control_widget_->Show();
}

AppNonClientFrameViewAsh::~AppNonClientFrameViewAsh() {
  frame()->RemoveObserver(frame_observer_.get());
  // This frame view can be replaced (and deleted) if the window is restored
  // via a keyboard shortcut like Alt-[.  Ensure we close the control widget.
  CloseControlWidget();
}

gfx::Rect AppNonClientFrameViewAsh::GetBoundsForClientView() const {
  return GetLocalBounds();
}

gfx::Rect AppNonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int AppNonClientFrameViewAsh::NonClientHitTest(
    const gfx::Point& point) {
  return HTNOWHERE;
}

void AppNonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* window_mask) {
}

void AppNonClientFrameViewAsh::ResetWindowControls() {
}

void AppNonClientFrameViewAsh::UpdateWindowIcon() {
}

void AppNonClientFrameViewAsh::UpdateWindowTitle() {
}

gfx::Rect AppNonClientFrameViewAsh::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  return gfx::Rect();
}

BrowserNonClientFrameView::TabStripInsets
AppNonClientFrameViewAsh::GetTabStripInsets(bool restored) const {
  return TabStripInsets();
}

int AppNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return 0;
}

void AppNonClientFrameViewAsh::UpdateThrobber(bool running) {
}

const char* AppNonClientFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

void AppNonClientFrameViewAsh::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (control_widget_)
    control_widget_->GetNativeView()->SetBounds(GetControlBounds());
}

gfx::Rect AppNonClientFrameViewAsh::GetControlBounds() const {
  if (!control_view_)
    return gfx::Rect();
  gfx::Size preferred = control_view_->GetPreferredSize();
  return gfx::Rect(
      base::i18n::IsRTL() ? 0 : (width() - preferred.width()), 0,
      preferred.width(), preferred.height());
}

void AppNonClientFrameViewAsh::CloseControlWidget() {
  if (control_widget_) {
    control_widget_->Close();
    control_widget_ = NULL;
  }
}
