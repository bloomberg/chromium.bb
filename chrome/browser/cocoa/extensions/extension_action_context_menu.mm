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
class AsyncUninstaller : public base::RefCountedThreadSafe<AsyncUninstaller>,
                         public ExtensionInstallUI::Delegate {
 public:
  AsyncUninstaller(Extension* extension) : extension_(extension) {}

  // Load the uninstall icon then show the confirmation dialog when finished.
  void LoadExtensionIconThenConfirmUninstall() {
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &AsyncUninstaller::LoadIconOnFileThread,
            &uninstall_icon_));
  }

  void Cancel() {
    uninstall_icon_.reset();
    extension_ = NULL;
  }

  // Overridden by ExtensionInstallUI::Delegate.
  virtual void InstallUIProceed() {
    Browser* browser = BrowserList::GetLastActive();
    // GetLastActive() returns NULL during testing.
    if (!browser)
      return;
    browser->profile()->GetExtensionsService()->
        UninstallExtension(extension_->id(), false);
  }

  virtual void InstallUIAbort() {}

 private:
  friend class base::RefCountedThreadSafe<AsyncUninstaller>;
  ~AsyncUninstaller() {}

  void LoadIconOnFileThread(scoped_ptr<SkBitmap>* uninstall_icon) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
    Extension::DecodeIcon(extension_, Extension::EXTENSION_ICON_LARGE,
                          uninstall_icon);
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
       NewRunnableMethod(this, &AsyncUninstaller::ShowConfirmationDialog,
           uninstall_icon));
  }

  void ShowConfirmationDialog(scoped_ptr<SkBitmap>* uninstall_icon) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    // If |extension_| is NULL, then the action was cancelled. Bail.
    if (!extension_)
      return;

    Browser* browser = BrowserList::GetLastActive();
    // GetLastActive() returns NULL during testing.
    if (!browser)
      return;

    ExtensionInstallUI client(browser->profile());
    client.ConfirmUninstall(this, extension_, uninstall_icon->get());
  }

  // The extension that we're loading the icon for. Weak.
  Extension* extension_;

  // The uninstall icon shown by the confirmation dialog.
  scoped_ptr<SkBitmap> uninstall_icon_;

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

- (id)initWithExtension:(Extension*)extension {
  if ((self = [super initWithTitle:@""])) {
    extension_ = extension;

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
          // setDisabled:NO does not work.
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
  Browser* browser = BrowserList::GetLastActive();
  // GetLastActive() returns NULL during testing.
  if (!browser)
    return;

  Profile* profile = browser->profile();

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
      browser->OpenURL(extension_->options_url(), GURL(),
                       NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    case kExtensionContextDisable: {
      ExtensionsService* extension_service = profile->GetExtensionsService();
      extension_service->DisableExtension(extension_->id());
      break;
    }
    case kExtensionContextUninstall: {
      if (uninstaller_.get())
        uninstaller_->Cancel();

      uninstaller_ = new AsyncUninstaller(extension_);
      uninstaller_->LoadExtensionIconThenConfirmUninstall();
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
