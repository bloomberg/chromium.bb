// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/number_formatting.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Maximum count of notification shown by a number label. "+" icon is shown
// instead if it exceeds this limit.
constexpr size_t kTrayNotificationMaxCount = 9;

const double kTrayNotificationCircleIconRadius = kTrayIconSize / 2 - 2;

// The size of the number font inside the icon. Should be updated when
// kTrayIconSize is changed.
constexpr int kNumberIconFontSize = 11;

gfx::FontList GetNumberIconFontList() {
  // |kNumberIconFontSize| is hard-coded as 11, which whould be updated when
  // the tray icon size is changed.
  DCHECK_EQ(16, kTrayIconSize);

  gfx::Font default_font;
  int font_size_delta = kNumberIconFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::BOLD);
  DCHECK_EQ(kNumberIconFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

class NumberIconImageSource : public gfx::CanvasImageSource {
 public:
  NumberIconImageSource(size_t count)
      : CanvasImageSource(gfx::Size(kTrayIconSize, kTrayIconSize), false),
        count_(count) {
    DCHECK_LE(count_, kTrayNotificationMaxCount + 1);
  }

  void Draw(gfx::Canvas* canvas) override {
    // Paint the contents inside the circle background. The color doesn't matter
    // as it will be hollowed out by the XOR operation.
    if (count_ > kTrayNotificationMaxCount) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(kSystemTrayNotificationCounterPlusIcon,
                                size().width(), kTrayIconColor),
          0, 0);
    } else {
      canvas->DrawStringRectWithFlags(
          base::FormatNumber(count_), GetNumberIconFontList(), kTrayIconColor,
          gfx::Rect(size()),
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    }
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kXor);
    flags.setAntiAlias(true);
    flags.setColor(kTrayIconColor);
    canvas->DrawCircle(gfx::RectF(gfx::SizeF(size())).CenterPoint(),
                       kTrayNotificationCircleIconRadius, flags);
  }

 private:
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(NumberIconImageSource);
};

}  // namespace

NotificationCounterView::NotificationCounterView() : TrayItemView(nullptr) {
  CreateImageView();
  Update();
}

NotificationCounterView::~NotificationCounterView() = default;

void NotificationCounterView::Update() {
  size_t notification_count =
      message_center::MessageCenter::Get()->NotificationCount();
  if (notification_count == 0 ||
      message_center::MessageCenter::Get()->IsQuietMode()) {
    SetVisible(false);
    return;
  }
  int icon_id = std::min(notification_count, kTrayNotificationMaxCount + 1);
  if (icon_id != count_for_display_) {
    image_view()->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(icon_id));
    count_for_display_ = icon_id;
  }
  SetVisible(true);
}

QuietModeView::QuietModeView() : TrayItemView(nullptr) {
  CreateImageView();
  // TODO(yamaguchi): Add this check when new style of the system tray is
  // implemented, so that icon resizing will not happen here.
  // DCHECK_EQ(kTrayIconSize,
  //     gfx::GetDefaultSizeOfVectorIcon(kSystemTrayDoNotDisturbIcon));
  image_view()->SetImage(gfx::CreateVectorIcon(kSystemTrayDoNotDisturbIcon,
                                               kTrayIconSize, kTrayIconColor));
  Update();
}

QuietModeView::~QuietModeView() = default;

void QuietModeView::Update() {
  SetVisible(message_center::MessageCenter::Get()->IsQuietMode());
}

}  // namespace ash
