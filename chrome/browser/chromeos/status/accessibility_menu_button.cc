// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/accessibility_menu_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/status/status_area_bubble.h"
#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace {

enum MenuItemID {
  MENU_ITEM_DISABLE_SPOKEN_FEEDBACK,
};

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// AccessibilityMenuButton

AccessibilityMenuButton::AccessibilityMenuButton(
    StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this) {
  set_id(VIEW_ID_STATUS_BUTTON_ACCESSIBILITY);
  accessibility_enabled_.Init(prefs::kAccessibilityEnabled,
                              g_browser_process->local_state(), this);
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUSBAR_ACCESSIBILITY));
  Update();
}

AccessibilityMenuButton::~AccessibilityMenuButton() {
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void AccessibilityMenuButton::RunMenu(views::View* source,
                                      const gfx::Point& pt) {
  PrepareMenu();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  CHECK(menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                                this, bounds, views::MenuItemView::TOPRIGHT,
                                0) ==
        views::MenuRunner::NORMAL_EXIT);
}

////////////////////////////////////////////////////////////////////////////////
// views::MenuDelegate implementation

void AccessibilityMenuButton::ExecuteCommand(int id) {
  switch (id) {
    case MENU_ITEM_DISABLE_SPOKEN_FEEDBACK:
      accessibility::EnableAccessibility(false, NULL);
      break;
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver implementation

void AccessibilityMenuButton::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    Update();
    const std::string path =
        *static_cast<content::Details<std::string> >(details).ptr();
    // Show a bubble when accessibility is turned on at the login screen.
    if (path == prefs::kAccessibilityEnabled) {
      if (accessibility_enabled_.GetValue() &&
          StatusAreaViewChromeos::IsLoginMode()) {
        views::ImageView* icon_view = new views::ImageView;
        const gfx::Image& image = ResourceBundle::GetSharedInstance().
            GetImageNamed(IDR_ACCESSIBILITY_ICON);
        icon_view->SetImage(image.ToSkBitmap());
        bubble_controller_.reset(
            StatusAreaBubbleController::ShowBubbleForAWhile(
                new StatusAreaBubbleContentView(
                    this,
                    icon_view,
                    l10n_util::GetStringUTF16(
                        IDS_STATUSBAR_ACCESSIBILITY_TURNED_ON_BUBBLE))));
      } else {
        bubble_controller_.reset();
      }
    }
  }
}


void AccessibilityMenuButton::Update() {
  // Update tooltip and accessibile name.
  string16 message =
      l10n_util::GetStringUTF16(IDS_STATUSBAR_ACCESSIBILITY_ENABLED);
  SetTooltipText(message);
  SetAccessibleName(message);
  // Update visibility.
  SetVisible(accessibility_enabled_.GetValue());
}

void AccessibilityMenuButton::PrepareMenu() {
  views::MenuItemView* menu = new views::MenuItemView(this);
  if (accessibility_enabled_.GetValue())
    menu->AppendMenuItemWithLabel(
        MENU_ITEM_DISABLE_SPOKEN_FEEDBACK,
        l10n_util::GetStringUTF16(IDS_STATUSBAR_DISABLE_SPOKEN_FEEDBACK));
  // |menu_runner_| takes the ownership of |menu|
  menu_runner_.reset(new views::MenuRunner(menu));
}

}  // namespace chromeos
