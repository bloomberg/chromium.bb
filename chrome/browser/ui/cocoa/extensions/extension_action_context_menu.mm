// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"

#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// A class that loads the extension icon on the I/O thread before showing the
// confirmation dialog to uninstall the given extension.
// Also acts as the extension's UI delegate in order to display the dialog.
class AsyncUninstaller : public ExtensionInstallUI::Delegate {
 public:
  AsyncUninstaller(const Extension* extension, Profile* profile)
      : extension_(extension),
        profile_(profile) {
    install_ui_.reset(new ExtensionInstallUI(profile));
    install_ui_->ConfirmUninstall(this, extension_);
  }

  ~AsyncUninstaller() {}

  // Overridden by ExtensionInstallUI::Delegate.
  virtual void InstallUIProceed() {
    profile_->GetExtensionService()->
        UninstallExtension(extension_->id(), false);
  }

  virtual void InstallUIAbort() {}

 private:
  // The extension that we're loading the icon for. Weak.
  const Extension* extension_;

  // The current profile. Weak.
  Profile* profile_;

  scoped_ptr<ExtensionInstallUI> install_ui_;

  DISALLOW_COPY_AND_ASSIGN(AsyncUninstaller);
};

namespace extension_action_context_menu {

class DevmodeObserver : public NotificationObserver {
 public:
  DevmodeObserver(ExtensionActionContextMenu* menu,
                             PrefService* service)
      : menu_(menu), pref_service_(service) {
    registrar_.Init(pref_service_);
    registrar_.Add(prefs::kExtensionsUIDeveloperMode, this);
  }
  virtual ~DevmodeObserver() {}

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [menu_ updateInspectorItem];
    else
      NOTREACHED();
  }

 private:
  ExtensionActionContextMenu* menu_;
  PrefService* pref_service_;
  PrefChangeRegistrar registrar_;
};

class ProfileObserverBridge : public NotificationObserver {
 public:
  ProfileObserverBridge(ExtensionActionContextMenu* owner,
                        const Profile* profile)
      : owner_(owner),
        profile_(profile) {
    registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                   Source<Profile>(profile));
  }

  ~ProfileObserverBridge() {}

  // Overridden from NotificationObserver
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::PROFILE_DESTROYED &&
        source == Source<Profile>(profile_)) {
      [owner_ invalidateProfile];
    }
  }

 private:
  ExtensionActionContextMenu* owner_;
  const Profile* profile_;
  NotificationRegistrar registrar_;
};

}  // namespace extension_action_context_menu

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
  kExtensionContextInspect = 8
};

int CurrentTabId() {
  Browser* browser = BrowserList::GetLastActive();
  if(!browser)
    return -1;
  TabContents* contents = browser->GetSelectedTabContents();
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
        l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_OPTIONS),
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

        // Disable the 'Options' item if there are no options to set.
        if ([itemObj tag] == kExtensionContextOptions &&
            extension_->options_url().spec().length() <= 0) {
          // Setting the target to nil will disable the item. For some reason
          // setEnabled:NO does not work.
          [itemObj setTarget:nil];
        } else {
          [itemObj setTarget:self];
        }

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

    NSString* inspectorTitle =
        l10n_util::GetNSStringWithFixup(IDS_EXTENSION_ACTION_INSPECT_POPUP);
    inspectorItem_.reset([[NSMenuItem alloc] initWithTitle:inspectorTitle
                                                    action:@selector(dispatch:)
                                             keyEquivalent:@""]);
    [inspectorItem_.get() setTarget:self];
    [inspectorItem_.get() setTag:kExtensionContextInspect];

    PrefService* service = profile_->GetPrefs();
    observer_.reset(
        new extension_action_context_menu::DevmodeObserver(self, service));
    profile_observer_.reset(
        new extension_action_context_menu::ProfileObserverBridge(self,
                                                                 profile));

    [self updateInspectorItem];
    return self;
  }
  return nil;
}

- (void)updateInspectorItem {
  PrefService* service = profile_->GetPrefs();
  bool devmode = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  if (devmode) {
    if ([self indexOfItem:inspectorItem_.get()] == -1)
      [self addItem:inspectorItem_.get()];
  } else {
    if ([self indexOfItem:inspectorItem_.get()] != -1)
      [self removeItem:inspectorItem_.get()];
  }
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
      ExtensionService* extensionService = profile_->GetExtensionService();
      if (!extensionService)
        return; // Incognito mode.
      extensionService->DisableExtension(extension_->id());
      break;
    }
    case kExtensionContextUninstall: {
      uninstaller_.reset(new AsyncUninstaller(extension_, profile_));
      break;
    }
    case kExtensionContextHide: {
      ExtensionService* extension_service = profile_->GetExtensionService();
      extension_service->SetBrowserActionVisibility(extension_, false);
      break;
    }
    case kExtensionContextManage: {
      browser->OpenURL(GURL(chrome::kChromeUIExtensionsURL), GURL(),
                       NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;
    }
    case kExtensionContextInspect: {
      BrowserWindowCocoa* window =
          static_cast<BrowserWindowCocoa*>(browser->window());
      ToolbarController* toolbarController =
          [window->cocoa_controller() toolbarController];
      LocationBarViewMac* locationBarView =
          [toolbarController locationBarBridge];

      NSPoint popupPoint = NSZeroPoint;
      if (extension_->page_action() == action_) {
        popupPoint = locationBarView->GetPageActionBubblePoint(action_);

      } else if (extension_->browser_action() == action_) {
        BrowserActionsController* controller =
            [toolbarController browserActionsController];
        popupPoint = [controller popupPointForBrowserAction:extension_];

      } else {
        NOTREACHED() << "action_ is not a page action or browser action?";
      }

      int tabId = CurrentTabId();
      GURL url = action_->GetPopupUrl(tabId);
      DCHECK(url.is_valid());
      [ExtensionPopupController showURL:url
                              inBrowser:BrowserList::GetLastActive()
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
  if([menuItem isEqualTo:inspectorItem_.get()]) {
    return action_ && action_->HasPopup(CurrentTabId());
  }
  return YES;
}

- (void)invalidateProfile {
  observer_.reset();
  profile_ = NULL;
}

@end
