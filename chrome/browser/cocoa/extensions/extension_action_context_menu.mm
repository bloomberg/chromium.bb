// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/extension_action_context_menu.h"

#include "app/l10n_util_mac.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

// A class that loads the extension icon on the I/O thread before showing the
// confirmation dialog to uninstall the given extension.
// Also acts as the extension's UI delegate in order to display the dialog.
class AsyncUninstaller : public ExtensionInstallUI::Delegate {
 public:
  AsyncUninstaller(Extension* extension, Profile* profile)
      : extension_(extension),
        profile_(profile) {
    install_ui_.reset(new ExtensionInstallUI(profile));
    install_ui_->ConfirmUninstall(this, extension_);
  }

  ~AsyncUninstaller() {}

  // Overridden by ExtensionInstallUI::Delegate.
  virtual void InstallUIProceed(bool create_shortcut) {
    DCHECK(!create_shortcut);
    profile_->GetExtensionsService()->
        UninstallExtension(extension_->id(), false);
  }

  virtual void InstallUIAbort() {}

 private:
  // The extension that we're loading the icon for. Weak.
  Extension* extension_;

  // The current profile. Weak.
  Profile* profile_;

  scoped_ptr<ExtensionInstallUI> install_ui_;

  DISALLOW_COPY_AND_ASSIGN(AsyncUninstaller);
};

@interface ExtensionActionContextMenu(Private)
// Callback for the context menu items.
- (void)dispatch:(id)menuItem;
@end

@implementation ExtensionActionContextMenu

namespace {
// Enum of menu item choices to their respective indices.
// NOTE: You MUST keep this in sync with the |menuItems| NSArray below.
enum {
  kExtensionContextName = 0,
  kExtensionContextOptions = 2,
  kExtensionContextDisable = 3,
  kExtensionContextUninstall = 4,
  kExtensionContextManage = 6
};
}  // namespace

- (id)initWithExtension:(Extension*)extension profile:(Profile*)profile {
  if ((self = [super initWithTitle:@""])) {
    extension_ = extension;
    profile_ = profile;

    NSArray* menuItems = [NSArray arrayWithObjects:
        base::SysUTF8ToNSString(extension->name()),
        [NSMenuItem separatorItem],
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_OPTIONS),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_DISABLE),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_UNINSTALL),
        [NSMenuItem separatorItem],
        l10n_util::GetNSStringWithFixup(IDS_MANAGE_EXTENSIONS),
        nil];

    for (id item in menuItems) {
      if ([item isKindOfClass:[NSMenuItem class]]) {
        [self addItem:item];
      } else if ([item isKindOfClass:[NSString class]]) {
        NSMenuItem* itemObj = [self addItemWithTitle:item
                                              action:@selector(dispatch:)
                                       keyEquivalent:@""];
        // The tag should correspond to the enum above.
        // NOTE: The enum and the order of the menu items MUST be in sync.
        [itemObj setTag:[self indexOfItem:itemObj]];

        // Disable the 'Options' item if there are no options to set.
        if ([itemObj tag] == kExtensionContextOptions &&
            extension_->options_url().spec().length() <= 0) {
          // Setting the target to nil will disable the item. For some reason
          // setEnabled:NO does not work.
          [itemObj setTarget:nil];
        } else {
          [itemObj setTarget:self];
        }
      }
    }

    return self;
  }
  return nil;
}

- (void)dispatch:(id)menuItem {
  Browser* browser = BrowserList::FindBrowserWithProfile(profile_);
  if (!browser)
    return;

  NSMenuItem* item = (NSMenuItem*)menuItem;
  switch ([item tag]) {
    case kExtensionContextName: {
      GURL url(std::string(extension_urls::kGalleryBrowsePrefix) +
               std::string("/detail/") + extension_->id());
      browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    case kExtensionContextOptions: {
      DCHECK(!extension_->options_url().is_empty());
      profile_->GetExtensionProcessManager()->OpenOptionsPage(extension_,
                                                              browser);
      break;
    }
    case kExtensionContextDisable: {
      ExtensionsService* extensionService = profile_->GetExtensionsService();
      if (!extensionService)
        return; // Incognito mode.
      extensionService->DisableExtension(extension_->id());
      break;
    }
    case kExtensionContextUninstall: {
      uninstaller_.reset(new AsyncUninstaller(extension_, profile_));
      break;
    }
    case kExtensionContextManage: {
      browser->OpenURL(GURL(chrome::kChromeUIExtensionsURL), GURL(),
                       NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

@end
