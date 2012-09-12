// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/action_box_menu.h"

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/browser/ui/views/browser_action_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

#if defined(OS_WIN) && !defined(USE_AURA)
// Included for MENU_POPUPITEM and a few other Windows specific constants.
#include <vssym32.h>
#include "ui/base/native_theme/native_theme_win.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// ActionBoxMenu

ActionBoxMenu::ActionBoxMenu(Browser* browser, ActionBoxMenuModel* model)
    : browser_(browser),
      root_(NULL),
      model_(model) {
}

ActionBoxMenu::~ActionBoxMenu() {
}

void ActionBoxMenu::Init() {
  DCHECK(!root_);
  root_ = new views::MenuItemView(this);
  root_->set_has_icons(true);
  PopulateMenu();
}

void ActionBoxMenu::RunMenu(views::MenuButton* menu_button) {
  gfx::Point screen_location;
  views::View::ConvertPointToScreen(menu_button, &screen_location);
  menu_runner_.reset(new views::MenuRunner(root_));
  // Ignore the result since we don't need to handle a deleted menu specially.
  ignore_result(
      menu_runner_->RunMenuAt(menu_button->GetWidget(),
                              menu_button,
                              gfx::Rect(screen_location, menu_button->size()),
                              views::MenuItemView::TOPRIGHT,
                              views::MenuRunner::HAS_MNEMONICS));
}

void ActionBoxMenu::ExecuteCommand(int id) {
  model_->ExecuteCommand(id);
}

views::Border* ActionBoxMenu::CreateMenuBorder() {
  // TODO(yefim): Use correct theme color on non-Windows.
  SkColor border_color = SK_ColorBLACK;
#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(yefim): Move to Windows only files if possible.
  border_color = ui::NativeThemeWin::instance()->GetThemeColorWithDefault(
      ui::NativeThemeWin::MENU, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR,
      COLOR_MENUTEXT);
#endif
  return views::Border::CreateSolidBorder(1, border_color);
}

views::Background* ActionBoxMenu::CreateMenuBackground() {
  // TODO(yefim): Use correct theme color on non-Windows.
  SkColor background_color = SK_ColorWHITE;
#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(yefim): Move to Windows only files if possible.
  background_color = ui::NativeThemeWin::instance()->GetThemeColorWithDefault(
      ui::NativeThemeWin::TEXTFIELD, EP_BACKGROUND, EBS_NORMAL,
      TMT_BACKGROUND, COLOR_WINDOW);
#endif
  return views::Background::CreateSolidBackground(background_color);
}

void ActionBoxMenu::InspectPopup(ExtensionAction* action) {
}

int ActionBoxMenu::GetCurrentTabId() const {
  return 0;
}

void ActionBoxMenu::OnBrowserActionExecuted(BrowserActionButton* button) {
}

void ActionBoxMenu::OnBrowserActionVisibilityChanged() {
}

gfx::Point ActionBoxMenu::GetViewContentOffset() const {
  return gfx::Point(0, 0);
}

bool ActionBoxMenu::NeedToShowMultipleIconStates() const {
  return false;
}

bool ActionBoxMenu::NeedToShowTooltip() const {
  return false;
}

void ActionBoxMenu::WriteDragDataForView(views::View* sender,
                                         const gfx::Point& press_pt,
                                         ui::OSExchangeData* data) {
}

int ActionBoxMenu::GetDragOperationsForView(views::View* sender,
                                            const gfx::Point& p) {
  return 0;
}

bool ActionBoxMenu::CanStartDragForView(views::View* sender,
                                        const gfx::Point& press_pt,
                                        const gfx::Point& p) {
  return false;
}

void ActionBoxMenu::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
}

void ActionBoxMenu::PopulateMenu() {
  for (int model_index = 0; model_index < model_->GetItemCount();
       ++model_index) {
    views::MenuItemView* menu_item = root_->AppendMenuItemFromModel(
        model_, model_index, model_->GetCommandIdAt(model_index));
    if (model_->GetTypeAt(model_index) == ui::MenuModel::TYPE_COMMAND) {
      if (model_->IsItemExtension(model_index)) {
        menu_item->SetMargins(0, 0);
        const extensions::Extension* extension =
            model_->GetExtensionAt(model_index);
        BrowserActionView* view = new BrowserActionView(extension,
            browser_, this);
        // |menu_item| will own the |view| from now on.
        menu_item->SetIconView(view);
      }
    }
  }
}
