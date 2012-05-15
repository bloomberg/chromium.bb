// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

enum MenuEntries {
  NAME = 0,
  CONFIGURE,
  HIDE,
  DISABLE,
  UNINSTALL,
  MANAGE
};

ExtensionContextMenuModel::ExtensionContextMenuModel(
    const Extension* extension,
    Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(SimpleMenuModel(this)),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()) {
  extension_action_ = extension->browser_action();
  if (!extension_action_)
    extension_action_ = extension->page_action();

  InitCommonCommands();
}

bool ExtensionContextMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExtensionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = this->GetExtension();
  if (!extension)
    return false;

  if (command_id == CONFIGURE) {
    return extension->options_url().spec().length() > 0;
  } else if (command_id == NAME) {
    // The NAME links to the Homepage URL. If the extension doesn't have a
    // homepage, we just disable this menu item.
    return extension->GetHomepageURL().is_valid();
  } else if (command_id == DISABLE || command_id == UNINSTALL) {
    // Some extension types can not be disabled or uninstalled.
    return Extension::UserMayDisable(extension->location());
  }
  return true;
}

bool ExtensionContextMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

void ExtensionContextMenuModel::ExecuteCommand(int command_id) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  switch (command_id) {
    case NAME: {
      OpenURLParams params(extension->GetHomepageURL(), Referrer(),
                           NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
                           false);
      browser_->OpenURL(params);
      break;
    }
    case CONFIGURE:
      DCHECK(!extension->options_url().is_empty());
      profile_->GetExtensionProcessManager()->OpenOptionsPage(extension,
                                                              browser_);
      break;
    case HIDE: {
      ExtensionService* extension_service = profile_->GetExtensionService();
      extension_service->extension_prefs()->
          SetBrowserActionVisibility(extension, false);
      break;
    }
    case DISABLE: {
      ExtensionService* extension_service = profile_->GetExtensionService();
      extension_service->DisableExtension(extension_id_,
                                          Extension::DISABLE_USER_ACTION);
      break;
    }
    case UNINSTALL: {
      AddRef();  // Balanced in Accepted() and Canceled()
      extension_uninstall_dialog_.reset(
          ExtensionUninstallDialog::Create(profile_, this));
      extension_uninstall_dialog_->ConfirmUninstall(extension);
      break;
    }
    case MANAGE: {
      browser_->ShowExtensionsTab();
      break;
    }
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

void ExtensionContextMenuModel::ExtensionUninstallAccepted() {
  if (GetExtension())
    profile_->GetExtensionService()->UninstallExtension(extension_id_, false,
                                                        NULL);

  Release();
}

void ExtensionContextMenuModel::ExtensionUninstallCanceled() {
  Release();
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitCommonCommands() {
  const Extension* extension = GetExtension();

  // The extension pointer should only be null if the extension was uninstalled,
  // and since the menu just opened, it should still be installed.
  DCHECK(extension);

  AddItem(NAME, UTF8ToUTF16(extension->name()));
  AddSeparator();
  AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);
  AddItemWithStringId(DISABLE, IDS_EXTENSIONS_DISABLE);
  AddItem(UNINSTALL, l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
  if (extension->browser_action())
    AddItemWithStringId(HIDE, IDS_EXTENSIONS_HIDE_BUTTON);
  AddSeparator();
  AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSIONS);
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  ExtensionService* extension_service = profile_->GetExtensionService();
  return extension_service->GetExtensionById(extension_id_, false);
}
