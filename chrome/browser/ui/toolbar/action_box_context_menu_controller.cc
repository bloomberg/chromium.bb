// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_context_menu_controller.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/window_open_disposition.h"

using extensions::Extension;

namespace {

// Takes care of running the uninstall extension dialog. This class deletes
// itself after the user has responded to the dialog.
class ExtensionUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionUninstaller(Browser* browser, Profile* profile);

  void ShowDialog(const Extension* extension);

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
  virtual ~ExtensionUninstaller();

  scoped_ptr<ExtensionUninstallDialog> dialog_;
  std::string extension_id_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstaller);
};

ExtensionUninstaller::ExtensionUninstaller(Browser* browser, Profile* profile)
    : dialog_(ExtensionUninstallDialog::Create(profile,
                                               browser,
                                               this)),
      profile_(profile) {
}

ExtensionUninstaller::~ExtensionUninstaller() {
}

void ExtensionUninstaller::ShowDialog(const Extension* extension) {
  extension_id_ = extension->id();
  dialog_->ConfirmUninstall(extension);
}

void ExtensionUninstaller::ExtensionUninstallAccepted() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (extension_service->GetInstalledExtension(extension_id_))
    extension_service->UninstallExtension(extension_id_, false, NULL);
  delete this;
}

void ExtensionUninstaller::ExtensionUninstallCanceled() {
  delete this;
}

}  // namespace

ActionBoxContextMenuController::ActionBoxContextMenuController(
    Browser* browser,
    const Extension* extension)
    : browser_(browser),
      extension_id_(extension->id()),
      menu_model_(this) {
  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  ReplaceChars(extension_name, "&", "&&", &extension_name);
  menu_model_.AddItem(IDC_ACTION_BOX_EXTENSION_HOMEPAGE,
                      UTF8ToUTF16(extension_name));
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_ACTION_BOX_EXTENSION_MANAGE,
                                  IDS_MANAGE_EXTENSION);
  menu_model_.AddItemWithStringId(IDC_ACTION_BOX_EXTENSION_UNINSTALL,
                                  IDS_EXTENSIONS_UNINSTALL);
}

ActionBoxContextMenuController::~ActionBoxContextMenuController() {
}

bool ActionBoxContextMenuController::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ActionBoxContextMenuController::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  switch (command_id) {
    case IDC_ACTION_BOX_EXTENSION_HOMEPAGE:
      return extensions::ManifestURL::GetHomepageURL(extension).is_valid();
    case IDC_ACTION_BOX_EXTENSION_MANAGE:
      return true;
    case IDC_ACTION_BOX_EXTENSION_UNINSTALL: {
      return extensions::ExtensionSystem::Get(browser_->profile())->
          management_policy()->UserMayModifySettings(extension, NULL);
    }
    default: {
      NOTREACHED();
      return false;
    }
  }
}

bool ActionBoxContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ActionBoxContextMenuController::ExecuteCommand(int command_id,
                                                    int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  switch (command_id) {
    case IDC_ACTION_BOX_EXTENSION_HOMEPAGE: {
      content::OpenURLParams params(
          extensions::ManifestURL::GetHomepageURL(extension),
          content::Referrer(), NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK, false);
      browser_->OpenURL(params);
      break;
    }
    case IDC_ACTION_BOX_EXTENSION_MANAGE: {
      chrome::ShowExtensions(browser_, extension->id());
      break;
    }
    case IDC_ACTION_BOX_EXTENSION_UNINSTALL: {
      ExtensionUninstaller* uninstaller =
          new ExtensionUninstaller(browser_, browser_->profile());
      uninstaller->ShowDialog(extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

const Extension* ActionBoxContextMenuController::GetExtension() const {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  return extension_service->GetInstalledExtension(extension_id_);
}
