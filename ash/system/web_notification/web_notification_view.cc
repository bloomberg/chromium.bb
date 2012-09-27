// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/web_notification/web_notification.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace message_center {

const SkColor kNotificationColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kNotificationReadColor = SkColorSetRGB(0xfa, 0xfa, 0xfa);

// Menu constants
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kShowSettingsCommand = 2;

// A dropdown menu for notifications.
class WebNotificationMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  explicit WebNotificationMenuModel(WebNotificationTray* tray,
                                    const WebNotification& notification)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
        tray_(tray),
        notification_(notification) {
    // Add 'disable notifications' menu item.
    if (!notification.extension_id.empty()) {
      AddItem(kToggleExtensionCommand,
              GetLabelForCommandId(kToggleExtensionCommand));
    } else if (!notification.display_source.empty()) {
      AddItem(kTogglePermissionCommand,
              GetLabelForCommandId(kTogglePermissionCommand));
    }
    // Add settings menu item.
    if (!notification.display_source.empty()) {
      AddItem(kShowSettingsCommand,
              GetLabelForCommandId(kShowSettingsCommand));
    }
  }

  virtual ~WebNotificationMenuModel() {
  }

  // Overridden from ui::SimpleMenuModel:
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_EXTENSIONS_DISABLE);
      case kTogglePermissionCommand:
        return l10n_util::GetStringFUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SITE_DISABLE,
            notification_.display_source);
      case kShowSettingsCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SETTINGS);
      default:
        NOTREACHED();
    }
    return string16();
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        tray_->DisableByExtension(notification_.id);
        break;
      case kTogglePermissionCommand:
        tray_->DisableByUrl(notification_.id);
        break;
      case kShowSettingsCommand:
        tray_->ShowSettings(notification_.id);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  WebNotificationTray* tray_;
  WebNotification notification_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationMenuModel);
};

WebNotificationView::WebNotificationView(WebNotificationTray* tray,
                                         const WebNotification& notification)
    : tray_(tray),
      notification_(notification),
      icon_(NULL),
      close_button_(NULL),
      scroller_(NULL),
      gesture_scroll_amount_(0.f) {
  InitView(tray, notification);
}

WebNotificationView::~WebNotificationView() {
}

void WebNotificationView::InitView(WebNotificationTray* tray,
                                   const WebNotification& notification) {
  set_border(views::Border::CreateSolidSidedBorder(
      1, 0, 0, 0, kBorderLightColor));
  SkColor bg_color = notification.is_read ?
      kNotificationReadColor : kNotificationColor;
  set_background(views::Background::CreateSolidBackground(bg_color));
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);

  icon_ = new views::ImageView;
  icon_->SetImageSize(
      gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize));
  icon_->SetImage(notification.image);

  views::Label* title = new views::Label(notification.title);
  title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
  views::Label* message = new views::Label(notification.message);
  message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  message->SetMultiLine(true);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AURA_UBER_TRAY_NOTIFY_CLOSE));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);

  const int padding_width = kTrayPopupPaddingHorizontal/2;
  columns->AddPaddingColumn(0, padding_width);

  // Notification Icon.
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kWebNotificationIconSize, kWebNotificationIconSize);

  columns->AddPaddingColumn(0, padding_width);

  // Notification message text.
  const int message_width = kWebNotificationWidth - kWebNotificationIconSize -
      kWebNotificationButtonWidth - (padding_width * 3);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                     100, /* resize percent */
                     views::GridLayout::FIXED, message_width, message_width);

  columns->AddPaddingColumn(0, padding_width);

  // Close button.
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kWebNotificationButtonWidth,
                     kWebNotificationButtonWidth);

  // Layout rows
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);

  layout->StartRow(0, 0);
  layout->AddView(icon_, 1, 2);
  layout->AddView(title, 1, 1);
  layout->AddView(close_button_, 1, 1);

  layout->StartRow(0, 0);
  layout->SkipColumns(2);
  layout->AddView(message, 1, 1);
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
}

bool WebNotificationView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.flags() & ui::EF_RIGHT_MOUSE_BUTTON) {
    ShowMenu(event.location());
    return true;
  }
  tray_->OnClicked(notification_.id);
  return true;
}

ui::EventResult WebNotificationView::OnGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP) {
    tray_->OnClicked(notification_.id);
    return ui::ER_CONSUMED;
  }

  if (event.type() == ui::ET_GESTURE_LONG_PRESS) {
    ShowMenu(event.location());
    return ui::ER_CONSUMED;
  }

  if (event.type() == ui::ET_SCROLL_FLING_START) {
    // The threshold for the fling velocity is computed empirically.
    // The unit is in pixels/second.
    const float kFlingThresholdForClose = 800.f;
    if (fabsf(event.details().velocity_x()) > kFlingThresholdForClose) {
      SlideOutAndClose(event.details().velocity_x() < 0 ? SLIDE_LEFT :
                       SLIDE_RIGHT);
    } else if (scroller_) {
      RestoreVisualState();
      scroller_->OnGestureEvent(event);
    }
    return ui::ER_CONSUMED;
  }

  if (!event.IsScrollGestureEvent())
    return ui::ER_UNHANDLED;

  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    gesture_scroll_amount_ = 0.f;
  } else if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    // The scroll-update events include the incremental scroll amount.
    gesture_scroll_amount_ += event.details().scroll_x();

    ui::Transform transform;
    transform.SetTranslateX(gesture_scroll_amount_);
    layer()->SetTransform(transform);
    layer()->SetOpacity(
        1.f - std::min(fabsf(gesture_scroll_amount_) / width(), 1.f));

  } else if (event.type() == ui::ET_GESTURE_SCROLL_END) {
    const float kScrollRatioForClosingNotification = 0.5f;
    float scrolled_ratio = fabsf(gesture_scroll_amount_) / width();
    if (scrolled_ratio >= kScrollRatioForClosingNotification)
      SlideOutAndClose(gesture_scroll_amount_ < 0 ? SLIDE_LEFT : SLIDE_RIGHT);
    else
      RestoreVisualState();
  }

  if (scroller_)
    scroller_->OnGestureEvent(event);
  return ui::ER_CONSUMED;
}

void WebNotificationView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == close_button_)
    tray_->SendRemoveNotification(notification_.id);
}

void WebNotificationView::OnImplicitAnimationsCompleted() {
  tray_->SendRemoveNotification(notification_.id);
}

void WebNotificationView::ShowMenu(gfx::Point screen_location) {
  WebNotificationMenuModel menu_model(tray_, notification_);
  if (menu_model.GetItemCount() == 0)
    return;

  views::MenuModelAdapter menu_model_adapter(&menu_model);
  views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());

  views::View::ConvertPointToScreen(this, &screen_location);
  ignore_result(menu_runner.RunMenuAt(
      GetWidget()->GetTopLevelWidget(),
      NULL,
      gfx::Rect(screen_location, gfx::Size()),
      views::MenuItemView::TOPRIGHT,
      views::MenuRunner::HAS_MNEMONICS));
}

void WebNotificationView::RestoreVisualState() {
  // Restore the layer state.
  const int kSwipeRestoreDurationMS = 150;
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSwipeRestoreDurationMS));
  layer()->SetTransform(ui::Transform());
  layer()->SetOpacity(1.f);
}

void WebNotificationView::SlideOutAndClose(SlideDirection direction) {
  const int kSwipeOutTotalDurationMS = 150;
  int swipe_out_duration = kSwipeOutTotalDurationMS * layer()->opacity();
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(swipe_out_duration));
  settings.AddObserver(this);

  ui::Transform transform;
  transform.SetTranslateX(direction == SLIDE_LEFT ? -width() : width());
  layer()->SetTransform(transform);
  layer()->SetOpacity(0.f);
}

}  // namespace message_center

}  // namespace ash
