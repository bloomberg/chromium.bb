// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

AppInfoFooterPanel::AppInfoFooterPanel(Profile* profile,
                                       const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      create_shortcuts_button_(NULL),
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

  if (remove_button_)
    AddChildView(remove_button_);
}

void AppInfoFooterPanel::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == create_shortcuts_button_) {
    CreateShortcuts();
  } else if (sender == remove_button_) {
    UninstallApp();
  } else {
    NOTREACHED();
  }
}

void AppInfoFooterPanel::ExtensionUninstallAccepted() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->UninstallExtension(app_->id(), false, NULL);

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
// ChromeOS can pin apps to the app launcher, but can't create shortcuts.
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void AppInfoFooterPanel::UninstallApp() {
  DCHECK(CanUninstallApp());
  extension_uninstall_dialog_.reset(
      extensions::ExtensionUninstallDialog::Create(profile_, NULL, this));
  extension_uninstall_dialog_->ConfirmUninstall(app_);
}

bool AppInfoFooterPanel::CanUninstallApp() const {
  return extensions::ExtensionSystem::Get(profile_)
      ->management_policy()
      ->UserMayModifySettings(app_, NULL);
}
