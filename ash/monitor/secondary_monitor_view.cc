// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/secondary_monitor_view.h"

#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

// Colors for the background, the message text and the shortcut text.
const SkColor kBackgroundColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kMessageColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);
const SkColor kShortcutColor = SkColorSetRGB(0x8f, 0x8f, 0x8f);

// A view to be displayed on secondary monitor.
class SecondaryMonitorView : public views::WidgetDelegateView {
 public:
  SecondaryMonitorView() {
    Init();
  }
  virtual ~SecondaryMonitorView() {
  }

  void Init() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    message_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_SECONDARY_MONITOR));
    message_->SetAutoColorReadabilityEnabled(false);
    message_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
    message_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    message_->SetEnabledColor(kMessageColor);
    AddChildView(message_);

    // TODO(oshima): Add image for fullscreen shortcut.
    shortcut_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_SECONDARY_MONITOR_SHORTCUT));
    shortcut_->SetAutoColorReadabilityEnabled(false);
    shortcut_->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
    shortcut_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    shortcut_->SetEnabledColor(kShortcutColor);
    AddChildView(shortcut_);
  }

  virtual void Layout() {
    const int kMessageMargin = 20;
    const float kShortcutPositionFromBottom = 50;
    gfx::Rect b = bounds();
    int bottom = bounds().height() - kShortcutPositionFromBottom;
    int shortcut_height = shortcut_->GetHeightForWidth(b.width());
    shortcut_->SetBounds(0, bottom, b.width(), shortcut_height);

    int msg_height = message_->GetHeightForWidth(b.width());
    bottom -= msg_height + kMessageMargin;
    message_->SetBounds(0, bottom, bounds().width(), msg_height);
  }

 private:
  views::Label* message_;
  views::Label* shortcut_;

  DISALLOW_COPY_AND_ASSIGN(SecondaryMonitorView);
};

}  // namespace

views::Widget* CreateSecondaryMonitorWidget(aura::Window* parent) {
  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  SecondaryMonitorView* view = new SecondaryMonitorView();
  params.delegate = view;
  params.parent = parent;
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(view);
  desktop_widget->Show();
  desktop_widget->GetNativeView()->SetName("SecondaryMonitor");
  return desktop_widget;
}

}  // namespace ash
