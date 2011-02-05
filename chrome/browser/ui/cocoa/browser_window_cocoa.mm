// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/bug_report_window_controller.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#import "chrome/browser/ui/cocoa/html_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/importer/import_settings_dialog.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/options/content_settings_dialog_controller.h"
#import "chrome/browser/ui/cocoa/options/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/options/keyword_editor_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "chrome/browser/ui/cocoa/repost_form_warning_mac.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/theme_install_bubble_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/rect.h"

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller,
                                       NSWindow* window)
  : browser_(browser),
    controller_(controller),
    confirm_close_factory_(browser) {
  // This pref applies to all windows, so all must watch for it.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::SIDEBAR_CHANGED,
                 NotificationService::AllSources());
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
}

void BrowserWindowCocoa::Show() {
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behaviour under
  // Windows, but |-makeKeyAndOrderFront:| won't send |-windowDidBecomeMain:|
  // until we return to the runloop. Therefore any calls to
  // |BrowserList::GetLastActive()| (for example, in bookmark_util), will return
  // the previous browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser_);

  [window() makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  NSRect cocoa_bounds = NSMakeRect(bounds.x(), 0, bounds.width(),
                                   bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  cocoa_bounds.origin.y =
      [screen frame].size.height - bounds.height() - bounds.y();

  [window() setFrame:cocoa_bounds display:YES];
}

// Callers assume that this doesn't immediately delete the Browser object.
// The controller implementing the window delegate methods called from
// |-performClose:| must take precautions to ensure that.
void BrowserWindowCocoa::Close() {
  // If there is an overlay window, we contain a tab being dragged between
  // windows. Don't hide the window as it makes the UI extra confused. We can
  // still close the window, as that will happen when the drag completes.
  if ([controller_ overlayWindow]) {
    [controller_ deferPerformClose];
  } else {
    // Make sure we hide the window immediately. Even though performClose:
    // calls orderOut: eventually, it leaves the window on-screen long enough
    // that we start to see tabs shutting down. http://crbug.com/23959
    // TODO(viettrungluu): This is kind of bad, since |-performClose:| calls
    // |-windowShouldClose:| (on its delegate, which is probably the
    // controller) which may return |NO| causing the window to not be closed,
    // thereby leaving a hidden window. In fact, our window-closing procedure
    // involves a (indirect) recursion on |-performClose:|, which is also bad.
    [window() orderOut:controller_];
    [window() performClose:controller_];
  }
}

void BrowserWindowCocoa::Activate() {
  [controller_ activate];
}

void BrowserWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::FlashFrame() {
  [NSApp requestUserAttention:NSInformationalRequest];
}

bool BrowserWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeHandle() {
  return window();
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return [controller_ statusBubble];
}

void BrowserWindowCocoa::SelectedTabToolbarSizeChanged(bool is_animating) {
  // According to beng, this is an ugly method that comes from the days when the
  // download shelf was a ChromeView attached to the TabContents, and as its
  // size changed via animation it notified through TCD/etc to the browser view
  // to relayout for each tick of the animation. We don't need anything of the
  // sort on Mac.
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NSString* newTitle =
      base::SysUTF16ToNSString(browser_->GetWindowTitleForCurrentTab());

  // Work around Cocoa bug: if a window changes title during the tracking of the
  // Window menu it doesn't display well and the constant re-sorting of the list
  // makes it difficult for the user to pick the desired window. Delay window
  // title updates until the default run-loop mode.

  if (pending_window_title_.get())
    [[NSRunLoop currentRunLoop]
        cancelPerformSelector:@selector(setTitle:)
                       target:window()
                     argument:pending_window_title_.get()];

  pending_window_title_.reset([newTitle copy]);
  [[NSRunLoop currentRunLoop]
      performSelector:@selector(setTitle:)
               target:window()
             argument:newTitle
                order:0
                modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
}

void BrowserWindowCocoa::ShelfVisibilityChanged() {
  // Mac doesn't yet support showing the bookmark bar at a different size on
  // the new tab page. When it does, this method should attempt to relayout the
  // bookmark bar/extension shelf as their preferred height may have changed.
  // http://crbug.com/43346
}

void BrowserWindowCocoa::UpdateDevTools() {
  [controller_ updateDevToolsForContents:
      browser_->GetSelectedTabContents()];
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  // Do nothing on Mac.
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [controller_ regularWindowFrame];
  gfx::Rect bounds(frame.origin.x, 0, frame.size.width, frame.size.height);
  bounds.set_y([screen frame].size.height - frame.origin.y - frame.size.height);
  return bounds;
}

bool BrowserWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

void BrowserWindowCocoa::SetFullscreen(bool fullscreen) {
  [controller_ setFullscreen:fullscreen];
}

bool BrowserWindowCocoa::IsFullscreen() const {
  return !![controller_ isFullscreen];
}

bool BrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
}

void BrowserWindowCocoa::ConfirmAddSearchProvider(
    const TemplateURL* template_url,
    Profile* profile) {
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile
                                                      delegate:NULL
                                                   templateURL:template_url];
  [NSApp beginSheet:[editor window]
     modalForWindow:window()
      modalDelegate:controller_
     didEndSelector:@selector(sheetDidEnd:returnCode:context:)
        contextInfo:NULL];
}

LocationBar* BrowserWindowCocoa::GetLocationBar() const {
  return [controller_ locationBarBridge];
}

void BrowserWindowCocoa::SetFocusToLocationBar(bool select_all) {
  [controller_ focusLocationBar:select_all ? YES : NO];
}

void BrowserWindowCocoa::UpdateReloadStopState(bool is_loading, bool force) {
  [controller_ setIsLoading:is_loading force:force];
}

void BrowserWindowCocoa::UpdateToolbar(TabContentsWrapper* contents,
                                       bool should_restore_state) {
  [controller_ updateToolbarWithContents:contents->tab_contents()
                      shouldRestoreState:should_restore_state ? YES : NO];
}

void BrowserWindowCocoa::FocusToolbar() {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusAppMenu() {
  // Chrome uses the standard Mac OS X menu bar, so this isn't needed.
}

void BrowserWindowCocoa::RotatePaneFocus(bool forwards) {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusBookmarksToolbar() {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusChromeOSStatus() {
  // Not needed on the Mac.
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BrowserWindowCocoa::IsBookmarkBarAnimating() const {
  return [controller_ isBookmarkBarAnimating];
}

bool BrowserWindowCocoa::IsTabStripEditable() const {
  return ![controller_ isDragSessionActive];
}

bool BrowserWindowCocoa::IsToolbarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

// This is called from Browser, which in turn is called directly from
// a menu option.  All we do here is set a preference.  The act of
// setting the preference sends notifications to all windows who then
// know what to do.
void BrowserWindowCocoa::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  return [controller_ addFindBar:find_bar_cocoa_controller];
}

views::Window* BrowserWindowCocoa::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
  return NULL;
}

void BrowserWindowCocoa::ShowUpdateChromeDialog() {
  restart_browser::RequestRestart(nil);
}

void BrowserWindowCocoa::ShowTaskManager() {
  TaskManagerMac::Show();
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  [controller_ showBookmarkBubbleForURL:url
                      alreadyBookmarked:(already_bookmarked ? YES : NO)];
}

bool BrowserWindowCocoa::IsDownloadShelfVisible() const {
  return [controller_ isDownloadShelfVisible] != NO;
}

DownloadShelf* BrowserWindowCocoa::GetDownloadShelf() {
  DownloadShelfController* shelfController = [controller_ downloadShelf];
  return [shelfController bridge];
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
  [ClearBrowsingDataController
      showClearBrowsingDialogForProfile:browser_->profile()];
}

void BrowserWindowCocoa::ShowImportDialog() {
  [ImportSettingsDialogController
          showImportSettingsDialogForProfile:browser_->profile()];
}

void BrowserWindowCocoa::ShowSearchEnginesDialog() {
  [KeywordEditorCocoaController showKeywordEditor:browser_->profile()];
}

void BrowserWindowCocoa::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
  RepostFormWarningMac::Create(GetNativeHandle(), tab_contents);
}

void BrowserWindowCocoa::ShowContentSettingsWindow(
    ContentSettingsType settings_type,
    Profile* profile) {
  [ContentSettingsDialogController showContentSettingsForType:settings_type
                                                      profile:profile];
}

void BrowserWindowCocoa::ShowCollectedCookiesDialog(TabContents* tab_contents) {
  // Deletes itself on close.
  new CollectedCookiesMac(GetNativeHandle(), tab_contents);
}

void BrowserWindowCocoa::ShowProfileErrorDialog(int message_id) {
  scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(IDS_OK)];
  [alert setMessageText:l10n_util::GetNSStringWithFixup(IDS_PRODUCT_NAME)];
  [alert setInformativeText:l10n_util::GetNSStringWithFixup(message_id)];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert runModal];
}

void BrowserWindowCocoa::ShowThemeInstallBubble() {
  ThemeInstallBubbleView::Show(window());
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  // Call InProgressDownloadResponse asynchronously to avoid a crash when the
  // browser window is closed here (http://crbug.com/44454).
  MessageLoop::current()->PostTask(
      FROM_HERE,
      confirm_close_factory_.NewRunnableMethod(
          &Browser::InProgressDownloadResponse,
          true));
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                        gfx::NativeWindow parent_window) {
  [HtmlDialogWindowController showHtmlDialog:delegate
                                     profile:browser_->profile()];
}

void BrowserWindowCocoa::UserChangedTheme() {
  [controller_ userChangedTheme];
}

int BrowserWindowCocoa::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserWindowCocoa::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowPageInfo(Profile* profile,
                                      const GURL& url,
                                      const NavigationEntry::SSLStatus& ssl,
                                      bool show_history) {
  browser::ShowPageInfoBubble(window(), profile, url, ssl, show_history);
}

void BrowserWindowCocoa::ShowAppMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

bool BrowserWindowCocoa::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return false;

  DCHECK(event.os_event != NULL);
  int id = GetCommandId(event);
  if (id == -1)
    return false;

  if (browser_->IsReservedCommand(id))
    return HandleKeyboardEventInternal(event.os_event);

  DCHECK(is_keyboard_shortcut != NULL);
  *is_keyboard_shortcut = true;

  return false;
}

void BrowserWindowCocoa::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return;

  DCHECK(event.os_event != NULL);
  HandleKeyboardEventInternal(event.os_event);
}

@interface MenuWalker : NSObject
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu;
@end

@implementation MenuWalker
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu {
  NSMenuItem* result = nil;

  for (NSMenuItem *item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = [self itemForKeyEquivalent:key
                                       menu:submenu];
    } else if ([item cr_firesForKeyEventIfEnabled:key]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}
@end

int BrowserWindowCocoa::GetCommandId(const NativeWebKeyboardEvent& event) {
  if ([event.os_event type] != NSKeyDown)
    return -1;

  // Look in menu.
  NSMenuItem* item = [MenuWalker itemForKeyEquivalent:event.os_event
                                                 menu:[NSApp mainMenu]];

  if (item && [item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in |Browser::IsReservedCommand()| anyhow.
  if (item && [item action] == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  // "Exit" doesn't use the |commandDispatch:| mechanism either.
  if (item && [item action] == @selector(terminate:))
    return IDC_EXIT;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event.os_event modifierFlags];
  const bool cmdKey = (modifiers & NSCommandKeyMask) != 0;
  const bool shiftKey = (modifiers & NSShiftKeyMask) != 0;
  const bool cntrlKey = (modifiers & NSControlKeyMask) != 0;
  const bool optKey = (modifiers & NSAlternateKeyMask) != 0;
  const int keyCode = [event.os_event keyCode];
  const unichar keyChar = KeyCharacterForEvent(event.os_event);

  int cmdNum = CommandForWindowKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode, keyChar);
  if (cmdNum != -1)
    return cmdNum;

  cmdNum = CommandForBrowserKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode, keyChar);
  if (cmdNum != -1)
    return cmdNum;

  return -1;
}

bool BrowserWindowCocoa::HandleKeyboardEventInternal(NSEvent* event) {
  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>(window());
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);

  // Do not fire shortcuts on key up.
  if ([event type] == NSKeyDown) {
    // Send the event to the menu before sending it to the browser/window
    // shortcut handling, so that if a user configures cmd-left to mean
    // "previous tab", it takes precedence over the built-in "history back"
    // binding. Other than that, the |-redispatchKeyEvent:| call would take care
    // of invoking the original menu item shortcut as well.

    if ([[NSApp mainMenu] performKeyEquivalent:event])
      return true;

    if ([event_window handleExtraBrowserKeyboardShortcut:event])
      return true;

    if ([event_window handleExtraWindowKeyboardShortcut:event])
      return true;

    if ([event_window handleDelayedWindowKeyboardShortcut:event])
      return true;
  }

  return [event_window redispatchKeyEvent:event];
}

void BrowserWindowCocoa::ShowCreateWebAppShortcutsDialog(
    TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowCreateChromeAppShortcutsDialog(
    Profile* profile, const Extension* app) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::Cut() {
  [NSApp sendAction:@selector(cut:) to:nil from:nil];
}

void BrowserWindowCocoa::Copy() {
  [NSApp sendAction:@selector(copy:) to:nil from:nil];
}

void BrowserWindowCocoa::Paste() {
  [NSApp sendAction:@selector(paste:) to:nil from:nil];
}

void BrowserWindowCocoa::ToggleTabStripMode() {
  [controller_ toggleTabStripDisplayMode];
}

void BrowserWindowCocoa::OpenTabpose() {
  [controller_ openTabpose];
}

void BrowserWindowCocoa::PrepareForInstant() {
  // TODO: implement fade as done on windows.
}

void BrowserWindowCocoa::ShowInstant(TabContents* preview_contents) {
  [controller_ showInstant:preview_contents];
}

void BrowserWindowCocoa::HideInstant(bool instant_is_active) {
  [controller_ hideInstant];

  // TODO: add support for |instant_is_active|.
}

gfx::Rect BrowserWindowCocoa::GetInstantBounds() {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect monitorFrame = [screen frame];
  NSRect frame = [controller_ instantFrame];
  gfx::Rect bounds(NSRectToCGRect(frame));
  bounds.set_y(NSHeight(monitorFrame) - bounds.y() - bounds.height());
  return bounds;
}

void BrowserWindowCocoa::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    // Only the key window gets a direct toggle from the menu.
    // Other windows hear about it from the notification.
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      [controller_ updateBookmarkBarVisibilityWithAnimation:YES];
      break;
    case NotificationType::SIDEBAR_CHANGED:
      UpdateSidebarForContents(
          Details<SidebarContainer>(details)->tab_contents());
      break;
    default:
      NOTREACHED();  // we don't ask for anything else!
      break;
  }
}

void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}

NSWindow* BrowserWindowCocoa::window() const {
  return [controller_ window];
}

void BrowserWindowCocoa::UpdateSidebarForContents(TabContents* tab_contents) {
  if (tab_contents == browser_->GetSelectedTabContents()) {
    [controller_ updateSidebarForContents:tab_contents];
  }
}
