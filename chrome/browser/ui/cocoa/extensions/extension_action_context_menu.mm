// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

// A class that loads the extension icon on the I/O thread before showing the
// confirmation dialog to uninstall the given extension.
// Also acts as the extension's UI delegate in order to display the dialog.
class AsyncUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  AsyncUninstaller(const Extension* extension, Profile* profile)
      : extension_(extension),
        profile_(profile) {
    extension_uninstall_dialog_.reset(
        ExtensionUninstallDialog::Create(profile, this));
    extension_uninstall_dialog_->ConfirmUninstall(extension_);
  }

  ~AsyncUninstaller() {}

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() {
    profile_->GetExtensionService()->
        UninstallExtension(extension_->id(), false, NULL);
  }
  virtual void ExtensionUninstallCanceled() {}

 private:
  // The extension that we're loading the icon for. Weak.
  const Extension* extension_;

  // The current profile. Weak.
  Profile* profile_;

  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

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
  kExtensionContextHide = 5,
  kExtensionContextManage = 7,
};

int CurrentTabId() {
  Browser* browser = BrowserList::GetLastActive();
  if(!browser)
    return -1;
  WebContents* contents = browser->GetSelectedWebContents();
  if (!contents)
    return -1;
  return ExtensionTabUtil::GetTabId(contents);
}

}  // namespace

- (id)initWithExtension:(const Extension*)extension
                profile:(Profile*)profile
        extensionAction:(ExtensionAction*)action{
  if ((self = [super initWithTitle:@""])) {
    action_ = action;
    extension_ = extension;
    profile_ = profile;

    NSArray* menuItems = [NSArray arrayWithObjects:
        base::SysUTF8ToNSString(extension->name()),
        [NSMenuItem separatorItem],
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_OPTIONS_MENU_ITEM),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_DISABLE),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_UNINSTALL),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_HIDE_BUTTON),
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

        // Only browser actions can have their button hidden. Page actions
        // should never show the "Hide" menu item.
        if ([itemObj tag] == kExtensionContextHide &&
            !extension->browser_action()) {
          [itemObj setTarget:nil];  // Item is disabled.
          [itemObj setHidden:YES];  // Item is hidden.
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
  Browser* browser = browser::FindBrowserWithProfile(profile_);
  if (!browser)
    return;

  NSMenuItem* item = (NSMenuItem*)menuItem;
  switch ([item tag]) {
    case kExtensionContextName: {
      GURL url(std::string(extension_urls::kGalleryBrowsePrefix) +
               std::string("/detail/") + extension_->id());
      OpenURLParams params(
          url, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
           false);
      browser->OpenURL(params);
      break;
    }
    case kExtensionContextOptions: {
      DCHECK(!extension_->options_url().is_empty());
      profile_->GetExtensionProcessManager()->OpenOptionsPage(extension_,
                                                              browser);
      break;
    }
    case kExtensionContextDisable: {
      ExtensionService* extensionService = profile_->GetExtensionService();
      if (!extensionService)
        return; // Incognito mode.
      extensionService->DisableExtension(extension_->id(),
                                         Extension::DISABLE_USER_ACTION);
      break;
    }
    case kExtensionContextUninstall: {
      uninstaller_.reset(new AsyncUninstaller(extension_, profile_));
      break;
    }
    case kExtensionContextHide: {
      ExtensionService* extension_service = profile_->GetExtensionService();
      extension_service->extension_prefs()->
          SetBrowserActionVisibility(extension_, false);
      break;
    }
    case kExtensionContextManage: {
      browser->ShowExtensionsTab();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  if ([menuItem tag] == kExtensionContextOptions) {
    // Disable 'Options' if there are no options to set.
    return extension_->options_url().spec().length() > 0;
  }
  return YES;
}

@end
