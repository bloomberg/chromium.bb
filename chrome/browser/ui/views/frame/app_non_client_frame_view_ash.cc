// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_non_client_frame_view_ash.h"

#include "ash/wm/workspace/frame_maximize_button.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/ash_resources.h"
#include "grit/generated_resources.h"  // Accessibility names
#include "grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {
// The number of pixels within the shadow to draw the buttons.
const int kShadowStart = 16;
// The size and close buttons are designed to overlap.
const int kButtonOverlap = 1;

// TODO(pkotwicz): Remove these constants once the IDR_AURA_FULLSCREEN_SHADOW
// resource is updated.
const int kShadowHeightStretch = -1;
}

class AppNonClientFrameViewAsh::ControlView
    : public views::View, public views::ButtonListener {
 public:
  explicit ControlView(AppNonClientFrameViewAsh* owner) :
      owner_(owner),
      close_button_(new views::ImageButton(this)),
      restore_button_(new ash::FrameMaximizeButton(this, owner_))
  {
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    restore_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_MAXIMIZE));

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    int control_base_resource_id = owner->browser_view()->IsOffTheRecord() ?
        IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_ACTIVE :
        IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
    control_base_ = rb.GetImageNamed(control_base_resource_id).ToImageSkia();
    shadow_ = rb.GetImageNamed(
        base::i18n::IsRTL() ? IDR_AURA_WINDOW_FULLSCREEN_SHADOW_RTL :
                              IDR_AURA_WINDOW_FULLSCREEN_SHADOW).ToImageSkia();

    AddChildView(close_button_);
    AddChildView(restore_button_);
  }

  virtual ~ControlView() {}

  virtual void Layout() OVERRIDE {
    restore_button_->SetPosition(gfx::Point(kShadowStart, 0));
    close_button_->SetPosition(gfx::Point(kShadowStart +
        restore_button_->width() - kButtonOverlap, 0));
  }

  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) OVERRIDE {
    if (is_add && child == this) {
      SetButtonImages(restore_button_,
                      IDR_AURA_WINDOW_FULLSCREEN_RESTORE,
                      IDR_AURA_WINDOW_FULLSCREEN_RESTORE_H,
                      IDR_AURA_WINDOW_FULLSCREEN_RESTORE_P);
      restore_button_->SizeToPreferredSize();

      SetButtonImages(close_button_,
                      IDR_AURA_WINDOW_FULLSCREEN_CLOSE,
                      IDR_AURA_WINDOW_FULLSCREEN_CLOSE_H,
                      IDR_AURA_WINDOW_FULLSCREEN_CLOSE_P);
      close_button_->SizeToPreferredSize();
    }
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(shadow_->width(),
                     shadow_->height() + kShadowHeightStretch);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->TileImageInt(*control_base_,
        base::i18n::IsRTL() ? 0 : restore_button_->x(),
        restore_button_->y(),
        restore_button_->width() - kButtonOverlap + close_button_->width(),
        restore_button_->height());

    views::View::OnPaint(canvas);

    canvas->DrawImageInt(*shadow_, 0, kShadowHeightStretch);
  }

  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_button_) {
      owner_->frame()->Close();
    } else if (sender == restore_button_) {
      restore_button_->SetState(views::CustomButton::STATE_NORMAL);
      owner_->frame()->Restore();
    }
  }

  // Returns the insets of the control which are only covered by the shadow.
  gfx::Insets GetShadowInsets() {
    bool rtl = base::i18n::IsRTL();
    return gfx::Insets(
        0,
        rtl ? 0 : restore_button_->x(),
        shadow_->height() - close_button_->height(),
        rtl ? restore_button_->x() : 0);
  }

 private:
  // Sets images whose ids are passed in for each of the respective states
  // of |button|.
  void SetButtonImages(views::ImageButton* button, int normal_image_id,
                       int hot_image_id, int pushed_image_id) {
    ui::ThemeProvider* theme_provider = GetThemeProvider();
    button->SetImage(views::CustomButton::STATE_NORMAL,
                     theme_provider->GetImageSkiaNamed(normal_image_id));
    button->SetImage(views::CustomButton::STATE_HOVERED,
                     theme_provider->GetImageSkiaNamed(hot_image_id));
    button->SetImage(views::CustomButton::STATE_PRESSED,
                     theme_provider->GetImageSkiaNamed(pushed_image_id));
  }

  AppNonClientFrameViewAsh* owner_;
  views::ImageButton* close_button_;
  views::ImageButton* restore_button_;
  const gfx::ImageSkia* control_base_;
  const gfx::ImageSkia* shadow_;

  DISALLOW_COPY_AND_ASSIGN(ControlView);
};

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
      control_view_(new ControlView(this)),
      control_widget_(NULL),
      frame_observer_(new FrameObserver(this)) {
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
  params.transparent = true;
  control_widget_->Init(params);
  control_widget_->SetContentsView(control_view_);
  aura::Window* window = control_widget_->GetNativeView();
  window->SetName(kControlWindowName);
  // Need to exclude the shadow from the active control area.
  window->SetHitTestBoundsOverrideOuter(control_view_->GetShadowInsets(), 1);
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

std::string AppNonClientFrameViewAsh::GetClassName() const {
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
