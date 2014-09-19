// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using extensions::Extension;

// A class that shows a confirmation dialog to uninstall the given extension.
// Also acts as the extension's UI delegate in order to display the dialog.
class AsyncUninstaller : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  AsyncUninstaller(const Extension* extension, Browser* browser)
      : extension_(extension),
        profile_(browser->profile()) {
    extension_uninstall_dialog_.reset(
        extensions::ExtensionUninstallDialog::Create(
            profile_, browser->window()->GetNativeWindow(), this));
    extension_uninstall_dialog_->ConfirmUninstall(extension_);
  }

  virtual ~AsyncUninstaller() {}

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->UninstallExtension(extension_->id(),
                             extensions::UNINSTALL_REASON_USER_INITIATED,
                             base::Bind(&base::DoNothing),
                             NULL);
  }
  virtual void ExtensionUninstallCanceled() OVERRIDE {}

 private:
  // The extension that's being uninstalled.
  const Extension* extension_;

  // The current profile. Weak.
  Profile* profile_;

  scoped_ptr<extensions::ExtensionUninstallDialog> extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(AsyncUninstaller);
};

@interface ExtensionActionContextMenuController ()
- (void)onExtensionName:(id)sender;
- (void)onAlwaysRun:(id)sender;
- (void)onOptions:(id)sender;
- (void)onUninstall:(id)sender;
- (void)onHide:(id)sender;
- (void)onManage:(id)sender;
- (void)onInspect:(id)sender;
@end

@implementation ExtensionActionContextMenuController

- (id)initWithExtension:(const Extension*)extension
                browser:(Browser*)browser
        extensionAction:(ExtensionAction*)action{
  if ((self = [super init])) {
    action_ = action;
    extension_ = extension;
    browser_ = browser;
  }
  return self;
}

- (void)populateMenu:(NSMenu*)menu {
  [menu setAutoenablesItems:NO];

  // Extension name.
  NSMenuItem* item =
      [menu addItemWithTitle:base::SysUTF8ToNSString(extension_->name())
                      action:@selector(onExtensionName:)
               keyEquivalent:@""];
  [item setTarget:self];
  [item setEnabled:extensions::ManifestURL::GetHomepageURL(
      extension_).is_valid()];

  // Separator.
  [menu addItem:[NSMenuItem separatorItem]];

  // Always Run. Only displayed if there is an active script injection action.
  content::WebContents* activeTab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (activeTab &&
      extensions::ActiveScriptController::GetForWebContents(activeTab)
          ->WantsToRun(extension_)) {
    item = [menu addItemWithTitle:
                l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_ALWAYS_RUN)
                           action:@selector(onAlwaysRun:)
                    keyEquivalent:@""];
    [item setTarget:self];
  }

  // Options.
  item = [menu addItemWithTitle:
              l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_OPTIONS_MENU_ITEM)
                         action:@selector(onOptions:)
                  keyEquivalent:@""];
  [item setTarget:self];
  [item setEnabled:extensions::OptionsPageInfo::GetOptionsPage(
      extension_).spec().length() > 0];


  // Uninstall.
  item = [menu addItemWithTitle:
              l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_UNINSTALL)
                         action:@selector(onUninstall:)
                  keyEquivalent:@""];
  [item setTarget:self];

  // Hide. Only used for browser actions.
  if (extensions::ExtensionActionManager::Get(
          browser_->profile())->GetBrowserAction(*extension_)) {
    item = [menu addItemWithTitle:
                l10n_util::GetNSStringWithFixup(IDS_EXTENSIONS_HIDE_BUTTON)
                           action:@selector(onHide:)
                    keyEquivalent:@""];
    [item setTarget:self];
  }

  // Separator.
  [menu addItem:[NSMenuItem separatorItem]];

  // Manage.
  item = [menu addItemWithTitle:
              l10n_util::GetNSStringWithFixup(IDS_MANAGE_EXTENSION)
                         action:@selector(onManage:)
                  keyEquivalent:@""];
  [item setTarget:self];

  // Inspect.
  PrefService* service = browser_->profile()->GetPrefs();
  bool devMode = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  if (devMode) {
    item = [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(
                IDS_EXTENSION_ACTION_INSPECT_POPUP)
                           action:@selector(onInspect:)
                    keyEquivalent:@""];
    [item setTarget:self];
  }
}

- (void)onExtensionName:(id)sender {
  GURL url(extension_urls::GetWebstoreItemDetailURLPrefix() + extension_->id());
  OpenURLParams params(url,
                       Referrer(),
                       NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK,
                       false);
  browser_->OpenURL(params);
}

- (void)onAlwaysRun:(id)sender {
  content::WebContents* activeTab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (activeTab) {
    extensions::ActiveScriptController::GetForWebContents(activeTab)
        ->AlwaysRunOnVisibleOrigin(extension_);
  }
}

- (void)onOptions:(id)sender {
  DCHECK(!extensions::OptionsPageInfo::GetOptionsPage(extension_).is_empty());
  extensions::ExtensionTabUtil::OpenOptionsPage(extension_, browser_);
}

- (void)onUninstall:(id)sender {
  uninstaller_.reset(new AsyncUninstaller(extension_, browser_));
}

- (void)onHide:(id)sender {
  extensions::ExtensionActionAPI::SetBrowserActionVisibility(
      extensions::ExtensionPrefs::Get(browser_->profile()),
      extension_->id(),
      false);
}

- (void)onManage:(id)sender {
  chrome::ShowExtensions(browser_, extension_->id());
}

- (void)onInspect:(id)sender {
  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser_->window());
  ToolbarController* toolbarController =
      [window->cocoa_controller() toolbarController];
  LocationBarViewMac* locationBarView = [toolbarController locationBarBridge];

  extensions::ExtensionActionManager* actionManager =
      extensions::ExtensionActionManager::Get(browser_->profile());
  NSPoint popupPoint = NSZeroPoint;
  if (actionManager->GetPageAction(*extension_) == action_) {
    popupPoint = locationBarView->GetPageActionBubblePoint(action_);
  } else if (actionManager->GetBrowserAction(*extension_) == action_) {
    BrowserActionsController* controller =
        [toolbarController browserActionsController];
    popupPoint = [controller popupPointForBrowserAction:extension_];
  } else {
    NOTREACHED() << "action_ is not a page action or browser action?";
  }

  content::WebContents* activeTab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!activeTab)
    return;

  int tabId = extensions::ExtensionTabUtil::GetTabId(activeTab);

  GURL url = action_->GetPopupUrl(tabId);
  if (!url.is_valid())
    return;

  [ExtensionPopupController showURL:url
                          inBrowser:browser_
                         anchoredAt:popupPoint
                      arrowLocation:info_bubble::kTopRight
                            devMode:YES];
}

@end
