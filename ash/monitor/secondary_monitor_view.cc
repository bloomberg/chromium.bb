// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/secondary_monitor_view.h"

#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

// Colors for the background, the message text and the shortcut text.
const SkColor kBackgroundColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kTextColor = SkColorSetRGB(127, 127, 127);

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
    message_->SetFont(rb.GetFont(ui::ResourceBundle::LargeFont));
    message_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    message_->SetEnabledColor(kTextColor);
    AddChildView(message_);

    shortcut_text_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_SECONDARY_MONITOR_SHORTCUT));
    shortcut_text_->SetAutoColorReadabilityEnabled(false);
    shortcut_text_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
    shortcut_text_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    shortcut_text_->SetEnabledColor(kTextColor);
    AddChildView(shortcut_text_);

    shortcut_image_ = new views::ImageView();
    shortcut_image_->SetImage(rb.GetImageSkiaNamed(IDR_AURA_SWITCH_MONITOR));
    AddChildView(shortcut_image_);
  }

  virtual void Layout() {
    const int kMessagePositionTopMargin = 40;
    const int kShortcutPositionBottomMargin = 40;
    const int kShortcutMargin = 4;  // margin between text and image.
    gfx::Rect b = bounds();

    int msg_height = message_->GetHeightForWidth(b.width());
    message_->SetBounds(
        0, kMessagePositionTopMargin, bounds().width(), msg_height);

    // TODO(oshima): Figure out what to do for RTL.
    // Align the shortcut text & image to the center.
    gfx::Size text_size = shortcut_text_->GetPreferredSize();
    gfx::Size image_size = shortcut_image_->GetPreferredSize();
    int height = std::max(text_size.height(), image_size.height());
    int y = b.height() - kShortcutPositionBottomMargin - height;
    int x = (b.width() -
             (text_size.width() + kShortcutMargin + image_size.width())) / 2;
    shortcut_text_->SetBounds(x, y + (height - text_size.height()) / 2,
                              text_size.width(), text_size.height());
    shortcut_image_->SetBounds(
        x + text_size.width() + kShortcutMargin,
        y + (height - image_size.height()) / 2,
        image_size.width(), image_size.height());
  }

 private:
  views::Label* message_;
  views::Label* shortcut_text_;
  views::ImageView* shortcut_image_;

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
