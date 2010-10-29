// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

enum MenuEntries {
  NAME = 0,
  CONFIGURE,
  DISABLE,
  UNINSTALL,
  MANAGE,
  INSPECT_POPUP
};

ExtensionContextMenuModel::ExtensionContextMenuModel(
    const Extension* extension,
    Browser* browser,
    PopupDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(SimpleMenuModel(this)),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate) {
  extension_action_ = extension->browser_action();
  if (!extension_action_)
    extension_action_ = extension->page_action();

  InitCommonCommands();

  if (profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode) &&
      delegate_) {
    AddSeparator();
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {
}

void ExtensionContextMenuModel::InitCommonCommands() {
  const Extension* extension = GetExtension();

  // The extension pointer should only be null if the extension was uninstalled,
  // and since the menu just opened, it should still be installed.
  DCHECK(extension);

  AddItem(NAME, UTF8ToUTF16(extension->name()));
  AddSeparator();
  AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS);
  AddItemWithStringId(DISABLE, IDS_EXTENSIONS_DISABLE);
  AddItemWithStringId(UNINSTALL, IDS_EXTENSIONS_UNINSTALL);
  AddSeparator();
  AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSIONS);
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
  } else if (command_id == INSPECT_POPUP) {
    TabContents* contents = browser_->GetSelectedTabContents();
    if (!contents)
      return false;

    return extension_action_->HasPopup(ExtensionTabUtil::GetTabId(contents));
  }
  return true;
}

bool ExtensionContextMenuModel::GetAcceleratorForCommandId(
    int command_id, menus::Accelerator* accelerator) {
  return false;
}

void ExtensionContextMenuModel::ExecuteCommand(int command_id) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  switch (command_id) {
    case NAME: {
      browser_->OpenURL(extension->GetHomepageURL(), GURL(),
                        NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    case CONFIGURE:
      DCHECK(!extension->options_url().is_empty());
      profile_->GetExtensionProcessManager()->OpenOptionsPage(extension,
                                                              browser_);
      break;
    case DISABLE: {
      ExtensionsService* extension_service = profile_->GetExtensionsService();
      extension_service->DisableExtension(extension_id_);
      break;
    }
    case UNINSTALL: {
      AddRef();  // Balanced in InstallUIProceed and InstallUIAbort.
      install_ui_.reset(new ExtensionInstallUI(profile_));
      install_ui_->ConfirmUninstall(this, extension);
      break;
    }
    case MANAGE: {
      browser_->OpenURL(GURL(chrome::kChromeUIExtensionsURL), GURL(),
                        SINGLETON_TAB, PageTransition::LINK);
      break;
    }
    case INSPECT_POPUP: {
      delegate_->InspectPopup(extension_action_);
      break;
    }
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

void ExtensionContextMenuModel::InstallUIProceed() {
  if (GetExtension())
    profile_->GetExtensionsService()->UninstallExtension(extension_id_, false);

  Release();
}

void ExtensionContextMenuModel::InstallUIAbort() {
  Release();
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  return extension_service->GetExtensionById(extension_id_, false);
}
