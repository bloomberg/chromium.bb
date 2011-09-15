// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/accessibility_menu_button.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/widget/widget.h"
#include "views/controls/menu/menu_runner.h"
#include "views/controls/menu/menu_item_view.h"

namespace {

enum MenuItemID {
  MENU_ITEM_DISABLE_SPOKEN_FEEDBACK,
};

}

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// AccessibilityMenuButton

AccessibilityMenuButton::AccessibilityMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
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
// NotificationObserver implementation

void AccessibilityMenuButton::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED)
    Update();
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
    menu->AppendMenuItemWithLabel(MENU_ITEM_DISABLE_SPOKEN_FEEDBACK,
                                  UTF16ToWide(l10n_util::GetStringUTF16(
                                      IDS_STATUSBAR_DISABLE_SPOKEN_FEEDBACK)));
  // |menu_runner_| takes the ownership of |menu|
  menu_runner_.reset(new views::MenuRunner(menu));
}

}  // namespace chromeos
