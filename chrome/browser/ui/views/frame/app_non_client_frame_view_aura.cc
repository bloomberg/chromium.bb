// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_non_client_frame_view_aura.h"

#include "base/debug/stack_trace.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {
// The number of pixels to use as a hover zone at the top of the screen.
const int kTopMargin = 1;
// How long the hover animation takes if uninterrupted.
const int kHoverFadeDurationMs = 130;
// The number of pixels within the shadow to draw the buttons.
const int kShadowStart = 28;
}

class AppNonClientFrameViewAura::ControlView
    : public views::View, public views::ButtonListener {
 public:
  explicit ControlView(AppNonClientFrameViewAura* owner) :
      owner_(owner),
      close_button_(CreateImageButton(IDR_AURA_WINDOW_FULLSCREEN_CLOSE)),
      restore_button_(CreateImageButton(IDR_AURA_WINDOW_FULLSCREEN_RESTORE)) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    separator_ =
        *rb.GetImageNamed(IDR_AURA_WINDOW_FULLSCREEN_SEPARATOR).ToSkBitmap();
    shadow_ = *rb.GetImageNamed(IDR_AURA_WINDOW_FULLSCREEN_SHADOW).ToSkBitmap();
    AddChildView(close_button_);
    AddChildView(restore_button_);
  }

  virtual void Layout() OVERRIDE {
    restore_button_->SetPosition(gfx::Point(kShadowStart, 0));
    close_button_->SetPosition(
        gfx::Point(kShadowStart + close_button_->width() + separator_.width(),
                   0));
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(shadow_.width(), shadow_.height());
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    views::View::OnPaint(canvas);
    canvas->DrawBitmapInt(
        separator_, restore_button_->x() + restore_button_->width(), 0);
    canvas->DrawBitmapInt(shadow_, 0, 0);
  }

  void ButtonPressed(
      views::Button* sender,
      const views::Event& event) OVERRIDE {
    if (sender == close_button_)
      owner_->Close();
    else if (sender == restore_button_)
      owner_->Restore();
  }

 private:
  // Gets an image representing 3 bitmaps laid out horizontally that will be
  // used as the normal, hot and pushed states for the created button.
  views::ImageButton* CreateImageButton(int resource_id) {
    views::ImageButton* button = new views::ImageButton(this);
    const SkBitmap* all_images =
        ResourceBundle::GetSharedInstance().GetImageNamed(
            resource_id).ToSkBitmap();
    int width = all_images->width() / 3;
    int height = all_images->height();

    SkBitmap normal, hot, pushed;
    all_images->extractSubset(
        &normal,
        SkIRect::MakeXYWH(0, 0, width, height));
    all_images->extractSubset(
        &hot,
        SkIRect::MakeXYWH(width, 0, width, height));
    all_images->extractSubset(
        &pushed,
        SkIRect::MakeXYWH(2 * width, 0, width, height));
    button->SetImage(views::CustomButton::BS_NORMAL, &normal);
    button->SetImage(views::CustomButton::BS_HOT, &hot);
    button->SetImage(views::CustomButton::BS_PUSHED, &pushed);

    button->SizeToPreferredSize();
    return button;
  }

  AppNonClientFrameViewAura* owner_;
  views::ImageButton* close_button_;
  views::ImageButton* restore_button_;
  SkBitmap separator_;
  SkBitmap shadow_;

  DISALLOW_COPY_AND_ASSIGN(ControlView);
};

class AppNonClientFrameViewAura::Host : public views::MouseWatcherHost {
 public:
  explicit Host(AppNonClientFrameViewAura* owner) : owner_(owner) {}
  virtual ~Host() {}

  virtual bool Contains(
      const gfx::Point& screen_point,
      views::MouseWatcherHost::MouseEventType type) {
    gfx::Rect top_margin = owner_->GetScreenBounds();
    top_margin.set_height(kTopMargin);
    gfx::Rect control_bounds = owner_->GetControlBounds();
    control_bounds.Inset(kShadowStart, 0, 0, kShadowStart);
    return top_margin.Contains(screen_point) ||
        control_bounds.Contains(screen_point);
  }

  AppNonClientFrameViewAura* owner_;

  DISALLOW_COPY_AND_ASSIGN(Host);
};

AppNonClientFrameViewAura::AppNonClientFrameViewAura(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      control_view_(new ControlView(this)),
      control_widget_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          mouse_watcher_(new Host(this), this)) {
}

AppNonClientFrameViewAura::~AppNonClientFrameViewAura() {
}

gfx::Rect AppNonClientFrameViewAura::GetBoundsForClientView() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(0, kTopMargin, 0,  0);
  return bounds;
}

gfx::Rect AppNonClientFrameViewAura::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect bounds = client_bounds;
  bounds.Inset(0, -kTopMargin, 0, 0);
  return bounds;
}

int AppNonClientFrameViewAura::NonClientHitTest(
    const gfx::Point& point) {
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

void AppNonClientFrameViewAura::GetWindowMask(const gfx::Size& size,
                                              gfx::Path* window_mask) {
}

void AppNonClientFrameViewAura::ResetWindowControls() {
}

void AppNonClientFrameViewAura::UpdateWindowIcon() {
}

gfx::Rect AppNonClientFrameViewAura::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  return gfx::Rect();
}

int AppNonClientFrameViewAura::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  return 0;
}

void AppNonClientFrameViewAura::UpdateThrobber(bool running) {
}

void AppNonClientFrameViewAura::OnMouseEntered(
    const views::MouseEvent& event) {
  if (!control_widget_) {
    control_widget_ = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.parent = browser_view()->GetNativeHandle();
    params.transparent = true;
    control_widget_->Init(params);
    control_widget_->SetContentsView(control_view_);
    aura::Window* window = control_widget_->GetNativeView();
    gfx::Rect control_bounds = GetControlBounds();
    control_bounds.set_y(control_bounds.y() - control_bounds.height());
    window->SetBounds(control_bounds);
    control_widget_->Show();
  }

  ui::Layer* layer = control_widget_->GetNativeView()->layer();
  ui::ScopedLayerAnimationSettings scoped_setter(layer->GetAnimator());
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kHoverFadeDurationMs));
  layer->SetBounds(GetControlBounds());
  layer->SetOpacity(1);

  mouse_watcher_.Start();
}

void AppNonClientFrameViewAura::MouseMovedOutOfHost() {
  ui::Layer* layer = control_widget_->GetNativeView()->layer();

  ui::ScopedLayerAnimationSettings scoped_setter(layer->GetAnimator());
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kHoverFadeDurationMs));
  gfx::Rect control_bounds = GetControlBounds();
  control_bounds.set_y(control_bounds.y() - control_bounds.height());
  layer->SetBounds(control_bounds);
  layer->SetOpacity(0);
}

gfx::Rect AppNonClientFrameViewAura::GetControlBounds() const {
  gfx::Size preferred = control_view_->GetPreferredSize();
  gfx::Point location(width() - preferred.width(), 0);
  ConvertPointToWidget(this, &location);
  return gfx::Rect(
      location.x(), location.y(),
      preferred.width(), preferred.height());
}

void AppNonClientFrameViewAura::Close() {
  if (control_widget_)
    control_widget_->Close();
  control_widget_ = NULL;
  mouse_watcher_.Stop();
  frame()->Close();
}

void AppNonClientFrameViewAura::Restore() {
  if (control_widget_)
    control_widget_->Close();
  control_widget_ = NULL;
  mouse_watcher_.Stop();
  frame()->Restore();
}
