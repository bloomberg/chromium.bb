// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/exit_warning_handler.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

const int64 kTimeOutMilliseconds = 2000;
const SkColor kForegroundColor = 0xFFFFFFFF;
const SkColor kBackgroundColor = 0xE0808080;
const int kHorizontalMarginAroundText = 100;
const int kVerticalMarginAroundText = 100;

class ExitWarningLabel : public views::Label {
 public:
  ExitWarningLabel() {}

  virtual ~ExitWarningLabel() {}

 private:
  virtual void PaintText(gfx::Canvas* canvas,
                         const string16& text,
                         const gfx::Rect& text_bounds,
                         int flags) OVERRIDE {
    // Turn off subpixel rendering.
    views::Label::PaintText(canvas,
                            text,
                            text_bounds,
                            flags | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

  DISALLOW_COPY_AND_ASSIGN(ExitWarningLabel);
};

class ExitWarningWidgetDelegateView : public views::WidgetDelegateView {
 public:
  ExitWarningWidgetDelegateView() : text_width_(0), width_(0), height_(0) {
    text_ = l10n_util::GetStringUTF16(IDS_ASH_EXIT_WARNING_POPUP_TEXT);
    accessible_name_ =
        l10n_util::GetStringUTF16(IDS_ASH_EXIT_WARNING_POPUP_TEXT_ACCESSIBLE);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    font_ = rb.GetFont(ui::ResourceBundle::LargeFont);
    text_width_ = font_.GetStringWidth(text_);
    width_ = text_width_ + kHorizontalMarginAroundText;
    height_ = font_.GetHeight() + kVerticalMarginAroundText;
    views::Label* label = new ExitWarningLabel;
    label->SetText(text_);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label->SetFont(font_);
    label->SetEnabledColor(kForegroundColor);
    label->SetDisabledColor(kForegroundColor);
    label->SetAutoColorReadabilityEnabled(false);
    AddChildView(label);
    SetLayoutManager(new views::FillLayout);
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(width_, height_);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), kBackgroundColor);
    views::WidgetDelegateView::OnPaint(canvas);
  }

  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = accessible_name_;
    state->role = ui::AccessibilityTypes::ROLE_ALERT;
  }

 private:
  base::string16 text_;
  base::string16 accessible_name_;
  gfx::Font font_;
  int text_width_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(ExitWarningWidgetDelegateView);
};

}  // namespace

ExitWarningHandler::ExitWarningHandler()
    : state_(IDLE),
      stub_timer_for_test_(false) {
}

ExitWarningHandler::~ExitWarningHandler() {
  // Note: If a timer is outstanding, it is stopped in its destructor.
  Hide();
}

void ExitWarningHandler::HandleAccelerator() {
  ShellDelegate* shell_delegate = Shell::GetInstance()->delegate();
  switch (state_) {
    case IDLE:
      state_ = WAIT_FOR_DOUBLE_PRESS;
      Show();
      StartTimer();
      shell_delegate->RecordUserMetricsAction(UMA_ACCEL_EXIT_FIRST_Q);
      break;
    case WAIT_FOR_DOUBLE_PRESS:
      state_ = EXITING;
      CancelTimer();
      Hide();
      shell_delegate->RecordUserMetricsAction(UMA_ACCEL_EXIT_SECOND_Q);
      shell_delegate->Exit();
      break;
    case EXITING:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExitWarningHandler::TimerAction() {
  Hide();
  if (state_ == WAIT_FOR_DOUBLE_PRESS)
    state_ = IDLE;
}

void ExitWarningHandler::StartTimer() {
  if (stub_timer_for_test_)
    return;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kTimeOutMilliseconds),
               this,
               &ExitWarningHandler::TimerAction);
}

void ExitWarningHandler::CancelTimer() {
  timer_.Stop();
}

void ExitWarningHandler::Show() {
  if (widget_)
    return;
  aura::RootWindow* root_window = Shell::GetActiveRootWindow();
  ExitWarningWidgetDelegateView* delegate = new ExitWarningWidgetDelegateView;
  gfx::Size rs = root_window->bounds().size();
  gfx::Size ps = delegate->GetPreferredSize();
  gfx::Rect bounds((rs.width() - ps.width()) / 2,
                   (rs.height() - ps.height()) / 3,
                   ps.width(), ps.height());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;
  params.can_activate = false;
  params.keep_on_top = true;
  params.remove_standard_frame = true;
  params.delegate = delegate;
  params.bounds = bounds;
  params.parent = Shell::GetContainer(
      root_window,
      internal::kShellWindowId_SettingBubbleContainer);
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->SetContentsView(delegate);
  widget_->GetNativeView()->SetName("ExitWarningWindow");
  widget_->Show();

  delegate->NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_ALERT, true);
}

void ExitWarningHandler::Hide() {
  widget_.reset();
}

}  // namespace ash
