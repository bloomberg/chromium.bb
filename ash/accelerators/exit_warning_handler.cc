// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/exit_warning_handler.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time.h"
#include "base/timer.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
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

const int64 kDoublePressTimeOutMilliseconds = 300;
const int64 kHoldTimeOutMilliseconds = 1700;
const SkColor kForegroundColor = 0xFFFFFFFF;
const SkColor kBackgroundColor = 0xE0808080;
const int kHorizontalMarginAroundText = 100;
const int kVerticalMarginAroundText = 100;

class ExitWarningWidgetDelegateView : public views::WidgetDelegateView {
 public:
  ExitWarningWidgetDelegateView() : text_width_(0), width_(0), height_(0) {
    text_ = l10n_util::GetStringUTF16(IDS_ASH_EXIT_WARNING_POPUP_TEXT);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    font_ = rb.GetFont(ui::ResourceBundle::LargeFont);
    text_width_ = font_.GetStringWidth(text_);
    width_ = text_width_ + kHorizontalMarginAroundText;
    height_ = font_.GetHeight() + kVerticalMarginAroundText;
    views::Label* label = new views::Label;
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

 private:
  base::string16 text_;
  gfx::Font font_;
  int text_width_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(ExitWarningWidgetDelegateView);
};

} // namespace

ExitWarningHandler::ExitWarningHandler()
  : state_(IDLE),
    widget_(NULL),
    stub_timers_for_test_(false) {
}

ExitWarningHandler::~ExitWarningHandler() {
  // Note: If a timer is outstanding, it is stopped in its destructor.
  Hide();
}

void ExitWarningHandler::HandleExitKey(bool press) {
  switch (state_) {
    case IDLE:
      if (press) {
        state_ = WAIT_FOR_QUICK_RELEASE;
        Show();
        StartTimers();
      }
      break;
    case WAIT_FOR_QUICK_RELEASE:
      if (!press)
        state_ = WAIT_FOR_DOUBLE_PRESS;
      break;
    case WAIT_FOR_DOUBLE_PRESS:
      if (press) {
        state_ = EXITING;
        CancelTimers();
        Hide();
        Shell::GetInstance()->delegate()->Exit();
      }
      break;
    case WAIT_FOR_LONG_HOLD:
      if (!press)
        state_ = CANCELED;
      break;
    case CANCELED:
    case EXITING:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExitWarningHandler::Timer1Action() {
  if (state_ == WAIT_FOR_QUICK_RELEASE)
    state_ = WAIT_FOR_LONG_HOLD;
  else if (state_ == WAIT_FOR_DOUBLE_PRESS)
    state_ = CANCELED;
}

void ExitWarningHandler::Timer2Action() {
  if (state_ == CANCELED) {
    state_ = IDLE;
    Hide();
  }
  else if (state_ == WAIT_FOR_LONG_HOLD) {
    state_ = EXITING;
    Hide();
    Shell::GetInstance()->delegate()->Exit();
  }
}

void ExitWarningHandler::StartTimers() {
  if (stub_timers_for_test_)
    return;
  timer1_.Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(
                    kDoublePressTimeOutMilliseconds),
                this,
                &ExitWarningHandler::Timer1Action);
  timer2_.Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(kHoldTimeOutMilliseconds),
                this,
                &ExitWarningHandler::Timer2Action);
}

void ExitWarningHandler::CancelTimers() {
  timer1_.Stop();
  timer2_.Stop();
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
  params.transient = true;
  params.transparent = true;
  params.accept_events = false;
  params.can_activate = false;
  params.keep_on_top = true;
  params.remove_standard_frame = true;
  params.delegate = delegate;
  params.bounds = bounds;
  params.parent = Shell::GetContainer(
      root_window,
      internal::kShellWindowId_SettingBubbleContainer);
  widget_ = new views::Widget;
  widget_->Init(params);
  widget_->SetContentsView(delegate);
  widget_->GetNativeView()->SetName("ExitWarningWindow");
  widget_->Show();
}

void ExitWarningHandler::Hide() {
  if (!widget_)
    return;
  widget_->Close();
  widget_ = NULL;
}

}  // namespace ash
