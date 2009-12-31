// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_context_menu_model.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

enum MenuEntries {
  NAME = 0,
  CONFIGURE,
  DISABLE,
  UNINSTALL,
  MANAGE,
};

ExtensionActionContextMenuModel::ExtensionActionContextMenuModel(
    Extension* extension)
  : ALLOW_THIS_IN_INITIALIZER_LIST(SimpleMenuModel(this)),
    extension_(extension) {
  AddItem(NAME, UTF8ToUTF16(extension->name()));
  AddSeparator();
  AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS);
  AddItemWithStringId(DISABLE, IDS_EXTENSIONS_DISABLE);
  AddItemWithStringId(UNINSTALL, IDS_EXTENSIONS_UNINSTALL);
  AddSeparator();

  // TODO(finnur): It is too late in the process to do another round of
  // translations, so we'll switch fully to IDS_MANAGE_EXTENSIONS after merging
  // this to the beta branch. :/
  std::string locale = g_browser_process->GetApplicationLocale();
  if (locale.find_first_of("en-") == 0)
    AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSIONS);
  else
    AddItemWithStringId(MANAGE, IDS_SHOW_EXTENSIONS);
}

ExtensionActionContextMenuModel::~ExtensionActionContextMenuModel() {
}

bool ExtensionActionContextMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExtensionActionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id == CONFIGURE)
    return extension_->options_url().spec().length() > 0;
  return true;
}

bool ExtensionActionContextMenuModel::GetAcceleratorForCommandId(
    int command_id, menus::Accelerator* accelerator) {
  return false;
}

void ExtensionActionContextMenuModel::ExecuteCommand(int command_id) {
  // TODO(finnur): GetLastActive returns NULL in unit tests.
  Browser* browser = BrowserList::GetLastActive();
  Profile* profile = browser->profile();

  switch (command_id) {
    case NAME: {
      GURL url(std::string(extension_urls::kGalleryBrowsePrefix) +
               std::string("/detail/") + extension_->id());
      browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    case CONFIGURE:
      DCHECK(!extension_->options_url().is_empty());
      browser->OpenURL(extension_->options_url(), GURL(),
                       NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    case DISABLE: {
      ExtensionsService* extension_service = profile->GetExtensionsService();
      extension_service->DisableExtension(extension_->id());
      break;
    }
    case UNINSTALL: {
      scoped_ptr<SkBitmap> uninstall_icon;
      Extension::DecodeIcon(extension_, Extension::EXTENSION_ICON_LARGE,
                            &uninstall_icon);

      ExtensionInstallUI client(profile);
      client.ConfirmUninstall(this, extension_, uninstall_icon.get());
      break;
    }
    case MANAGE: {
      browser->OpenURL(GURL(chrome::kChromeUIExtensionsURL), GURL(),
                       NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

void ExtensionActionContextMenuModel::InstallUIProceed() {
  // TODO(finnur): GetLastActive returns NULL in unit tests.
  Browser* browser = BrowserList::GetLastActive();
  std::string id = extension_->id();
  browser->profile()->GetExtensionsService()->UninstallExtension(id, false);
}
