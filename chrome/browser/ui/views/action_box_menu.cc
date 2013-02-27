// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/action_box_menu.h"

#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

using extensions::ActionInfo;
using extensions::Extension;
using extensions::IconImage;

namespace {
class ExtensionImageView : public views::ImageView, public IconImage::Observer {
 public:
  ExtensionImageView(Profile* profile, const Extension* extension) {
    const ActionInfo* page_launcher_info =
        ActionInfo::GetPageLauncherInfo(extension);
    icon_.reset(new IconImage(profile,
                              extension,
                              page_launcher_info->default_icon,
                              extension_misc::EXTENSION_ICON_ACTION,
                              extensions::IconsInfo::GetDefaultAppIcon(),
                              this));
    SetImage(icon_->image_skia());
  }

 private:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE {
    SetImage(icon_->image_skia());
  }

  scoped_ptr<extensions::IconImage> icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionImageView);
};
}  // namespace

// static
scoped_ptr<ActionBoxMenu> ActionBoxMenu::Create(
    Profile* profile,
    scoped_ptr<ActionBoxMenuModel> model) {
  scoped_ptr<ActionBoxMenu> menu(new ActionBoxMenu(profile, model.Pass()));
  menu->PopulateMenu();
  return menu.Pass();
}

ActionBoxMenu::~ActionBoxMenu() {
}

void ActionBoxMenu::RunMenu(views::MenuButton* menu_button,
                            gfx::Point menu_offset) {
  views::View::ConvertPointToScreen(menu_button, &menu_offset);

  // Ignore the result since we don't need to handle a deleted menu specially.
  ignore_result(
      menu_runner_->RunMenuAt(menu_button->GetWidget(),
                              menu_button,
                              gfx::Rect(menu_offset, menu_button->size()),
                              views::MenuItemView::TOPRIGHT,
                              views::MenuRunner::HAS_MNEMONICS));
}

ActionBoxMenu::ActionBoxMenu(Profile* profile,
                             scoped_ptr<ActionBoxMenuModel> model)
    : profile_(profile),
      model_(model.Pass()) {
  views::MenuItemView* menu = new views::MenuItemView(this);
  menu->set_has_icons(true);

  menu_runner_.reset(new views::MenuRunner(menu));
}

void ActionBoxMenu::ExecuteCommand(int id) {
  model_->ExecuteCommand(id);
}

void ActionBoxMenu::PopulateMenu() {
  for (int model_index = 0; model_index < model_->GetItemCount();
       ++model_index) {
    views::MenuItemView* menu_item =
        menu_runner_->GetMenu()->AppendMenuItemFromModel(
            model_.get(), model_index, model_->GetCommandIdAt(model_index));
    if (model_->GetTypeAt(model_index) == ui::MenuModel::TYPE_COMMAND) {
      if (model_->IsItemExtension(model_index)) {
        const Extension* extension = model_->GetExtensionAt(model_index);
        ExtensionImageView* view = new ExtensionImageView(profile_, extension);
        // |menu_item| will own the |view| from now on.
        menu_item->SetIconView(view);
      }
    }
  }
}
