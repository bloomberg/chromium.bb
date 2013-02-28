// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using extensions::Extension;

// A class that loads the extension icon on the I/O thread before showing the
// confirmation dialog to uninstall the given extension.
// Also acts as the extension's UI delegate in order to display the dialog.
class AsyncUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  AsyncUninstaller(const Extension* extension, Browser* browser)
      : extension_(extension),
        profile_(browser->profile()) {
    extension_uninstall_dialog_.reset(
        ExtensionUninstallDialog::Create(profile_, browser, this));
    extension_uninstall_dialog_->ConfirmUninstall(extension_);
  }

  virtual ~AsyncUninstaller() {}

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE {
    extensions::ExtensionSystem::Get(profile_)->extension_service()->
        UninstallExtension(extension_->id(), false, NULL);
  }
  virtual void ExtensionUninstallCanceled() OVERRIDE {}

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

// Runs the action for |menuItem|.
- (void)updateInspectorItem;
@end

namespace extension_action_context_menu {

class DevmodeObserver {
 public:
  DevmodeObserver(ExtensionActionContextMenu* menu,
                  PrefService* service)
      : menu_(menu), pref_service_(service) {
    registrar_.Init(pref_service_);
    registrar_.Add(
        prefs::kExtensionsUIDeveloperMode,
        base::Bind(&DevmodeObserver::OnExtensionsUIDeveloperModeChanged,
                   base::Unretained(this)));
  }
  virtual ~DevmodeObserver() {}

  void OnExtensionsUIDeveloperModeChanged() {
    [menu_ updateInspectorItem];
  }

 private:
  ExtensionActionContextMenu* menu_;
  PrefService* pref_service_;
  PrefChangeRegistrar registrar_;
};

}  // namespace extension_action_context_menu

@implementation ExtensionActionContextMenu

namespace {
// Enum of menu item choices to their respective indices.
// NOTE: You MUST keep this in sync with the |menuItems| NSArray below.
enum {
  kExtensionContextName = 0,
  kExtensionContextOptions = 2,
  kExtensionContextUninstall = 3,
  kExtensionContextHide = 4,
  kExtensionContextManage = 6,
  kExtensionContextInspect = 7
};

}  // namespace

- (id)initWithExtension:(const Extension*)extension
                browser:(Browser*)browser
        extensionAction:(ExtensionAction*)action{
  if ((self = [super initWithTitle:@""])) {
    action_ = action;
    extension_ = extension;
    browser_ = browser;

    // NOTE: You MUST keep this in sync with the enumeration in the header file.
    NSArray* menuItems = [NSArray arrayWithObjects:
        base::SysUTF8ToNSString(extension->name()),
        [NSMenuItem separatorItem],
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_OPTIONS_MENU_ITEM),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_UNINSTALL),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_HIDE_BUTTON),
        [NSMenuItem separatorItem],
        l10n_util::GetNSStringWithFixup(IDS_MANAGE_EXTENSION),
        l10n_util::GetNSStringWithFixup(IDS_EXTENSION_ACTION_INSPECT_POPUP),
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
            !extensions::ExtensionActionManager::Get(browser_->profile())->
            GetBrowserAction(*extension)) {
          [itemObj setTarget:nil];  // Item is disabled.
          [itemObj setHidden:YES];  // Item is hidden.
        } else {
          [itemObj setTarget:self];
        }
      }
    }

    PrefService* service = browser_->profile()->GetPrefs();
    observer_.reset(
        new extension_action_context_menu::DevmodeObserver(self, service));

    [self updateInspectorItem];
    return self;
  }
  return nil;
}

- (void)updateInspectorItem {
  PrefService* service = browser_->profile()->GetPrefs();
  bool devmode = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  NSMenuItem* item = [self itemWithTag:kExtensionContextInspect];
  [item setHidden:!devmode];
}

- (void)dispatch:(id)menuItem {
  NSMenuItem* item = (NSMenuItem*)menuItem;
  switch ([item tag]) {
    case kExtensionContextName: {
      GURL url(std::string(extension_urls::kGalleryBrowsePrefix) +
               std::string("/detail/") + extension_->id());
      OpenURLParams params(
          url, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
           false);
      browser_->OpenURL(params);
      break;
    }
    case kExtensionContextOptions: {
      DCHECK(!extensions::ManifestURL::GetOptionsPage(extension_).is_empty());
      extensions::ExtensionSystem::Get(browser_->profile())->process_manager()->
          OpenOptionsPage(extension_, browser_);
      break;
    }
    case kExtensionContextUninstall: {
      uninstaller_.reset(new AsyncUninstaller(extension_, browser_));
      break;
    }
    case kExtensionContextHide: {
      ExtensionService* extension_service = extensions::ExtensionSystem::Get(
          browser_->profile())->extension_service();
      extension_service->extension_prefs()->
          SetBrowserActionVisibility(extension_, false);
      break;
    }
    case kExtensionContextManage: {
      chrome::ShowExtensions(browser_, extension_->id());
      break;
    }
    case kExtensionContextInspect: {
      BrowserWindowCocoa* window =
          static_cast<BrowserWindowCocoa*>(browser_->window());
      ToolbarController* toolbarController =
          [window->cocoa_controller() toolbarController];
      LocationBarViewMac* locationBarView =
          [toolbarController locationBarBridge];

      extensions::ExtensionActionManager* action_manager =
          extensions::ExtensionActionManager::Get(browser_->profile());
      NSPoint popupPoint = NSZeroPoint;
      if (action_manager->GetPageAction(*extension_) == action_) {
        popupPoint = locationBarView->GetPageActionBubblePoint(action_);

      } else if (action_manager->GetBrowserAction(*extension_) == action_) {
        BrowserActionsController* controller =
            [toolbarController browserActionsController];
        popupPoint = [controller popupPointForBrowserAction:extension_];

      } else {
        NOTREACHED() << "action_ is not a page action or browser action?";
      }

      content::WebContents* active_tab =
          browser_->tab_strip_model()->GetActiveWebContents();
      if (!active_tab)
         break;

      int tabId = ExtensionTabUtil::GetTabId(active_tab);

      GURL url = action_->GetPopupUrl(tabId);
      if (!url.is_valid())
        return;

      [ExtensionPopupController showURL:url
                              inBrowser:browser_
                             anchoredAt:popupPoint
                          arrowLocation:info_bubble::kTopRight
                                devMode:YES];
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
    return extensions::ManifestURL::
        GetOptionsPage(extension_).spec().length() > 0;
  }
  return YES;
}

@end
