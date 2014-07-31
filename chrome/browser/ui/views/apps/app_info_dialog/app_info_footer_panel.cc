// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"

#include "ash/shelf/shelf_delegate.h"
#include "ash/shell.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

AppInfoFooterPanel::AppInfoFooterPanel(gfx::NativeWindow parent_window,
                                       Profile* profile,
                                       const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      parent_window_(parent_window),
      create_shortcuts_button_(NULL),
      pin_to_shelf_button_(NULL),
      unpin_from_shelf_button_(NULL),
      remove_button_(NULL),
      weak_ptr_factory_(this) {
  CreateButtons();

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        views::kButtonHEdgeMargin,
                                        views::kButtonVEdgeMargin,
                                        views::kRelatedButtonHSpacing));

  LayoutButtons();
}

AppInfoFooterPanel::~AppInfoFooterPanel() {
}

void AppInfoFooterPanel::CreateButtons() {
  if (CanCreateShortcuts()) {
    create_shortcuts_button_ = new views::LabelButton(
        this,
        l10n_util::GetStringUTF16(
            IDS_APPLICATION_INFO_CREATE_SHORTCUTS_BUTTON_TEXT));
    create_shortcuts_button_->SetStyle(views::Button::STYLE_BUTTON);
  }

  if (CanSetPinnedToShelf()) {
    pin_to_shelf_button_ = new views::LabelButton(
        this, l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN));
    pin_to_shelf_button_->SetStyle(views::Button::STYLE_BUTTON);
    unpin_from_shelf_button_ = new views::LabelButton(
        this, l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN));
    unpin_from_shelf_button_->SetStyle(views::Button::STYLE_BUTTON);
  }

  if (CanUninstallApp()) {
    remove_button_ = new views::LabelButton(
        this,
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_UNINSTALL_BUTTON_TEXT));
    remove_button_->SetStyle(views::Button::STYLE_BUTTON);
  }
}

void AppInfoFooterPanel::LayoutButtons() {
  if (create_shortcuts_button_)
    AddChildView(create_shortcuts_button_);

  if (pin_to_shelf_button_)
    AddChildView(pin_to_shelf_button_);
  if (unpin_from_shelf_button_)
    AddChildView(unpin_from_shelf_button_);
  UpdatePinButtons();

  if (remove_button_)
    AddChildView(remove_button_);
}

void AppInfoFooterPanel::UpdatePinButtons() {
  if (pin_to_shelf_button_ && unpin_from_shelf_button_) {
    bool is_pinned =
        !ash::Shell::GetInstance()->GetShelfDelegate()->IsAppPinned(app_->id());
    pin_to_shelf_button_->SetVisible(is_pinned);
    unpin_from_shelf_button_->SetVisible(!is_pinned);
  }
}

void AppInfoFooterPanel::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == create_shortcuts_button_) {
    CreateShortcuts();
  } else if (sender == pin_to_shelf_button_) {
    SetPinnedToShelf(true);
  } else if (sender == unpin_from_shelf_button_) {
    SetPinnedToShelf(false);
  } else if (sender == remove_button_) {
    UninstallApp();
  } else {
    NOTREACHED();
  }
}

void AppInfoFooterPanel::ExtensionUninstallAccepted() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->UninstallExtension(app_->id(),
                              extensions::UNINSTALL_REASON_USER_INITIATED,
                              base::Bind(&base::DoNothing),
                              NULL);

  // Close the App Info dialog as well (which will free the dialog too).
  GetWidget()->Close();
}

void AppInfoFooterPanel::ExtensionUninstallCanceled() {
  extension_uninstall_dialog_.reset();
}

void AppInfoFooterPanel::CreateShortcuts() {
  DCHECK(CanCreateShortcuts());
  chrome::ShowCreateChromeAppShortcutsDialog(GetWidget()->GetNativeWindow(),
                                             profile_,
                                             app_,
                                             base::Callback<void(bool)>());
}

bool AppInfoFooterPanel::CanCreateShortcuts() const {
  // Ash platforms can't create shortcuts.
  return (chrome::GetHostDesktopTypeForNativeWindow(parent_window_) !=
          chrome::HOST_DESKTOP_TYPE_ASH);
}

void AppInfoFooterPanel::SetPinnedToShelf(bool value) {
  DCHECK(CanSetPinnedToShelf());
  ash::ShelfDelegate* shelf_delegate =
      ash::Shell::GetInstance()->GetShelfDelegate();
  DCHECK(shelf_delegate);
  if (value)
    shelf_delegate->PinAppWithID(app_->id());
  else
    shelf_delegate->UnpinAppWithID(app_->id());

  UpdatePinButtons();
  Layout();
}

bool AppInfoFooterPanel::CanSetPinnedToShelf() const {
  // Non-Ash platforms don't have a shelf.
  if (chrome::GetHostDesktopTypeForNativeWindow(parent_window_) !=
      chrome::HOST_DESKTOP_TYPE_ASH) {
    return false;
  }

  // The Chrome app can't be unpinned.
  return app_->id() != extension_misc::kChromeAppId &&
         ash::Shell::GetInstance()->GetShelfDelegate()->CanPin();
}

void AppInfoFooterPanel::UninstallApp() {
  DCHECK(CanUninstallApp());
  extension_uninstall_dialog_.reset(
      extensions::ExtensionUninstallDialog::Create(
          profile_, GetWidget()->GetNativeWindow(), this));
  extension_uninstall_dialog_->ConfirmUninstall(app_);
}

bool AppInfoFooterPanel::CanUninstallApp() const {
  return extensions::ExtensionSystem::Get(profile_)
      ->management_policy()
      ->UserMayModifySettings(app_, NULL);
}
