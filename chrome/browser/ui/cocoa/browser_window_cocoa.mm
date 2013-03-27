// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/browser/password_generation_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/web_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/website_settings_bubble_controller.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/password_form.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/rect.h"

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"
#import "chrome/browser/ui/cocoa/one_click_signin_dialog_controller.h"
#endif

using content::NativeWebKeyboardEvent;
using content::SSLStatus;
using content::WebContents;

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSWindowAnimationBehaviorDefault = 0,
  NSWindowAnimationBehaviorNone = 2,
  NSWindowAnimationBehaviorDocumentWindow = 3,
  NSWindowAnimationBehaviorUtilityWindow = 4,
  NSWindowAnimationBehaviorAlertPanel = 5
};
typedef NSInteger NSWindowAnimationBehavior;

@interface NSWindow (LionSDKDeclarations)
- (NSWindowAnimationBehavior)animationBehavior;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
@end

#endif  // MAC_OS_X_VERSION_10_7

namespace {

NSPoint GetPointForBubble(content::WebContents* web_contents,
                          int x_offset,
                          int y_offset) {
  NSView* view = web_contents->GetView()->GetNativeView();
  NSRect bounds = [view bounds];
  NSPoint point;
  point.x = NSMinX(bounds) + x_offset;
  // The view's origin is at the bottom but |rect|'s origin is at the top.
  point.y = NSMaxY(bounds) - y_offset;
  point = [view convertPoint:point toView:nil];
  point = [[view window] convertBaseToScreen:point];
  return point;
}

void CreateShortcuts(const ShellIntegration::ShortcutInfo& shortcut_info) {
  // creation_locations will be ignored by CreatePlatformShortcuts on Mac.
  ShellIntegration::ShortcutLocations creation_locations;
  web_app::CreateShortcuts(shortcut_info, creation_locations);
}

}  // namespace

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller)
  : browser_(browser),
    controller_(controller),
    confirm_close_factory_(browser),
    initial_show_state_(ui::SHOW_STATE_DEFAULT),
    attention_request_id_(0) {

  gfx::Rect bounds;
  chrome::GetSavedWindowBoundsAndShowState(browser_,
                                           &bounds,
                                           &initial_show_state_);

  browser_->search_model()->AddObserver(this);
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
  browser_->search_model()->RemoveObserver(this);
}

void BrowserWindowCocoa::Show() {
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behaviour under
  // Windows, but |-makeKeyAndOrderFront:| won't send |-windowDidBecomeMain:|
  // until we return to the runloop. Therefore any calls to
  // |chrome::FindLastActiveWithHostDesktopType| will return the previous
  // browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser_);

  bool is_session_restore = browser_->is_session_restore();
  NSWindowAnimationBehavior saved_animation_behavior =
      NSWindowAnimationBehaviorDefault;
  bool did_save_animation_behavior = false;
  // Turn off swishing when restoring windows.
  if (is_session_restore &&
      [window() respondsToSelector:@selector(animationBehavior)] &&
      [window() respondsToSelector:@selector(setAnimationBehavior:)]) {
    did_save_animation_behavior = true;
    saved_animation_behavior = [window() animationBehavior];
    [window() setAnimationBehavior:NSWindowAnimationBehaviorNone];
  }

  [window() makeKeyAndOrderFront:controller_];

  // When creating windows from nibs it is necessary to |makeKeyAndOrderFront:|
  // prior to |orderOut:| then |miniaturize:| when restoring windows in the
  // minimized state.
  if (initial_show_state_ == ui::SHOW_STATE_MINIMIZED) {
    [window() orderOut:controller_];
    [window() miniaturize:controller_];
  } else if (initial_show_state_ == ui::SHOW_STATE_FULLSCREEN) {
    chrome::ToggleFullscreenMode(browser_);
  }
  initial_show_state_ = ui::SHOW_STATE_DEFAULT;

  // Restore window animation behavior.
  if (did_save_animation_behavior)
    [window() setAnimationBehavior:saved_animation_behavior];

  browser_->OnWindowDidShow();
}

void BrowserWindowCocoa::ShowInactive() {
  [window() orderFront:controller_];
}

void BrowserWindowCocoa::Hide() {
  // Not implemented.
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect real_bounds = [controller_ enforceMinWindowSize:bounds];

  ExitFullscreen();
  NSRect cocoa_bounds = NSMakeRect(real_bounds.x(), 0,
                                   real_bounds.width(),
                                   real_bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  cocoa_bounds.origin.y =
      NSHeight([screen frame]) - real_bounds.height() - real_bounds.y();

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
    // Using |-performClose:| can prevent the window from actually closing if
    // a JavaScript beforeunload handler opens an alert during shutdown, as
    // documented at <http://crbug.com/118424>. Re-implement
    // -[NSWindow performClose:] as closely as possible to how Apple documents
    // it.
    //
    // Before calling |-close|, hide the window immediately. |-performClose:|
    // would do something similar, and this ensures that the window is removed
    // from AppKit's display list. Not doing so can lead to crashes like
    // <http://crbug.com/156101>.
    id<NSWindowDelegate> delegate = [window() delegate];
    SEL window_should_close = @selector(windowShouldClose:);
    if ([delegate respondsToSelector:window_should_close]) {
      if ([delegate windowShouldClose:window()]) {
        [window() orderOut:nil];
        [window() close];
      }
    } else if ([window() respondsToSelector:window_should_close]) {
      if ([window() performSelector:window_should_close withObject:window()]) {
        [window() orderOut:nil];
        [window() close];
      }
    } else {
      [window() orderOut:nil];
      [window() close];
    }
  }
}

void BrowserWindowCocoa::Activate() {
  [controller_ activate];
}

void BrowserWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

bool BrowserWindowCocoa::IsAlwaysOnTop() const {
  return false;
}

bool BrowserWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeWindow() {
  return window();
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return [controller_ statusBubble];
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NSString* newTitle =
      base::SysUTF16ToNSString(browser_->GetWindowTitleForCurrentTab());

  pending_window_title_.reset(
      [BrowserWindowUtils scheduleReplaceOldTitle:pending_window_title_.get()
                                     withNewTitle:newTitle
                                        forWindow:window()]);
}

void BrowserWindowCocoa::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  [[controller_ bookmarkBarController]
      updateState:browser_->bookmark_bar_state()
       changeType:change_type];
}

void BrowserWindowCocoa::UpdateDevTools() {
  [controller_ updateDevToolsForContents:
      browser_->tab_strip_model()->GetActiveWebContents()];
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  // Do nothing on Mac.
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
}

void BrowserWindowCocoa::ZoomChangedForActiveTab(bool can_show_bubble) {
  [controller_ zoomChangedForActiveTab:can_show_bubble ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [controller_ regularWindowFrame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

gfx::Rect BrowserWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

bool BrowserWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

bool BrowserWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

void BrowserWindowCocoa::Maximize() {
  // Zoom toggles so only call if not already maximized.
  if (!IsMaximized())
    [window() zoom:controller_];
}

void BrowserWindowCocoa::Minimize() {
  [window() miniaturize:controller_];
}

void BrowserWindowCocoa::Restore() {
  if (IsMaximized())
    [window() zoom:controller_];  // Toggles zoom mode.
  else if (IsMinimized())
    [window() deminiaturize:controller_];
}

void BrowserWindowCocoa::EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType bubble_type) {
  [controller_ enterPresentationModeForURL:url
                                bubbleType:bubble_type];
}

void BrowserWindowCocoa::ExitFullscreen() {
  [controller_ exitFullscreen];
}

void BrowserWindowCocoa::UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) {
  [controller_ updateFullscreenExitBubbleURL:url bubbleType:bubble_type];
}

bool BrowserWindowCocoa::ShouldHideUIForFullscreen() const {
  // On Mac, fullscreen mode has most normal things (in a slide-down panel).
  return false;
}

bool BrowserWindowCocoa::IsFullscreen() const {
  if ([controller_ inPresentationMode])
    CHECK([controller_ isFullscreen]);  // Presentation mode must be fullscreen.
  return [controller_ isFullscreen];
}

bool BrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
}

void BrowserWindowCocoa::ConfirmAddSearchProvider(
    TemplateURL* template_url,
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

void BrowserWindowCocoa::UpdateToolbar(content::WebContents* contents,
                                       bool should_restore_state) {
  [controller_ updateToolbarWithContents:contents
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

gfx::Rect BrowserWindowCocoa::GetRootWindowResizerRect() const {
  if (IsDownloadShelfVisible())
    return gfx::Rect();
  NSRect tabRect = [controller_ selectedTabGrowBoxRect];
  return gfx::Rect(NSRectToCGRect(tabRect));
}

bool BrowserWindowCocoa::IsPanel() const {
  return false;
}

// This is called from Browser, which in turn is called directly from
// a menu option.  All we do here is set a preference.  The act of
// setting the preference sends notifications to all windows who then
// know what to do.
void BrowserWindowCocoa::ToggleBookmarkBar() {
  chrome::ToggleBookmarkBarWhenVisible(browser_->profile());
}

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  [controller_ addFindBar:find_bar_cocoa_controller];
}

void BrowserWindowCocoa::ShowUpdateChromeDialog() {
  restart_browser::RequestRestart(window());
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  [controller_ showBookmarkBubbleForURL:url
                      alreadyBookmarked:(already_bookmarked ? YES : NO)];
}

void BrowserWindowCocoa::ShowChromeToMobileBubble() {
  [controller_ showChromeToMobileBubble];
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void BrowserWindowCocoa::ShowOneClickSigninBubble(
    OneClickSigninBubbleType type,
    const StartSyncCallback& start_sync_callback) {
  if (type == ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE) {
    scoped_nsobject<OneClickSigninBubbleController> bubble_controller(
        [[OneClickSigninBubbleController alloc]
            initWithBrowserWindowController:cocoa_controller()
                                   callback:start_sync_callback]);
    [bubble_controller showWindow:nil];
  } else {
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    // Deletes itself when the dialog closes.
    new OneClickSigninDialogController(web_contents, start_sync_callback);
  }
}
#endif

bool BrowserWindowCocoa::IsDownloadShelfVisible() const {
  return [controller_ isDownloadShelfVisible] != NO;
}

DownloadShelf* BrowserWindowCocoa::GetDownloadShelf() {
  DownloadShelfController* shelfController = [controller_ downloadShelf];
  return [shelfController bridge];
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  // Call InProgressDownloadResponse asynchronously to avoid a crash when the
  // browser window is closed here (http://crbug.com/44454).
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&Browser::InProgressDownloadResponse,
                 confirm_close_factory_.GetWeakPtr(), true));
}

void BrowserWindowCocoa::UserChangedTheme() {
  [controller_ userChangedTheme];
}

int BrowserWindowCocoa::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserWindowCocoa::WebContentsFocused(WebContents* contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowWebsiteSettings(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const content::SSLStatus& ssl,
    bool show_history) {
  WebsiteSettingsUIBridge::Show(
      window(), profile, web_contents, url, ssl);
}

void BrowserWindowCocoa::ShowAppMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

bool BrowserWindowCocoa::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return false;

  if (event.type == WebKit::WebInputEvent::RawKeyDown &&
      [controller_ handledByExtensionCommand:event.os_event])
    return true;

  int id = [BrowserWindowUtils getCommandId:event];
  if (id == -1)
    return false;

  if (browser_->command_controller()->IsReservedCommandOrKey(id, event)) {
      return [BrowserWindowUtils handleKeyboardEvent:event.os_event
                                            inWindow:window()];
  }

  DCHECK(is_keyboard_shortcut);
  *is_keyboard_shortcut = true;
  return false;
}

void BrowserWindowCocoa::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ([BrowserWindowUtils shouldHandleKeyboardEvent:event])
    [BrowserWindowUtils handleKeyboardEvent:event.os_event inWindow:window()];
}

void BrowserWindowCocoa::ShowCreateChromeAppShortcutsDialog(
    Profile* profile, const extensions::Extension* app) {
  // Normally we would show a dialog, but since we always create the app
  // shortcut in /Applications there are no options for the user to choose.
  web_app::UpdateShortcutInfoAndIconForApp(*app, profile,
                                           base::Bind(&CreateShortcuts));
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

void BrowserWindowCocoa::OpenTabpose() {
  [controller_ openTabpose];
}

void BrowserWindowCocoa::EnterFullscreenWithChrome() {
  CHECK(base::mac::IsOSLionOrLater());
  if ([controller_ inPresentationMode])
    [controller_ exitPresentationMode];
  else
    [controller_ enterFullscreen];
}

bool BrowserWindowCocoa::IsFullscreenWithChrome() {
  return IsFullscreen() && ![controller_ inPresentationMode];
}

bool BrowserWindowCocoa::IsFullscreenWithoutChrome() {
  return IsFullscreen() && [controller_ inPresentationMode];
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

WindowOpenDisposition BrowserWindowCocoa::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  // In Lion fullscreen mode, convert popups into tabs.
  if (base::mac::IsOSLionOrLater() && IsFullscreen())
    return NEW_FOREGROUND_TAB;
  return NEW_POPUP;
}

FindBar* BrowserWindowCocoa::CreateFindBar() {
  // We could push the AddFindBar() call into the FindBarBridge
  // constructor or the FindBarCocoaController init, but that makes
  // unit testing difficult, since we would also require a
  // BrowserWindow object.
  FindBarBridge* bridge = new FindBarBridge(browser_);
  AddFindBar(bridge->find_bar_cocoa_controller());
  return bridge;
}

bool BrowserWindowCocoa::GetConstrainedWindowTopY(int* top_y) {
  return false;
}

extensions::ActiveTabPermissionGranter*
    BrowserWindowCocoa::GetActiveTabPermissionGranter() {
  WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return NULL;
  extensions::TabHelper* tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  return tab_helper ? tab_helper->active_tab_permission_granter() : NULL;
}

void BrowserWindowCocoa::ModelChanged(const SearchModel::State& old_state,
                                      const SearchModel::State& new_state) {
  [controller_ updateBookmarkBarStateForInstantOverlay];
}

void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}

NSWindow* BrowserWindowCocoa::window() const {
  return [controller_ window];
}

void BrowserWindowCocoa::ShowAvatarBubble(WebContents* web_contents,
                                          const gfx::Rect& rect) {
  NSPoint point = GetPointForBubble(web_contents, rect.right(), rect.bottom());

  // |menu| will automatically release itself on close.
  AvatarMenuBubbleController* menu =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  [[menu bubble] setAlignment:info_bubble::kAlignEdgeToAnchorEdge];
  [menu showWindow:nil];
}

void BrowserWindowCocoa::ShowAvatarBubbleFromAvatarButton() {
  [[controller_ avatarButtonController] showAvatarBubble];
}

void BrowserWindowCocoa::ShowPasswordGenerationBubble(
    const gfx::Rect& rect,
    const content::PasswordForm& form,
    autofill::PasswordGenerator* password_generator) {
  WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  // We want to point to the middle of the rect instead of the right side.
  NSPoint point = GetPointForBubble(web_contents,
                                    rect.x() + rect.width()/2,
                                    rect.bottom());

  PasswordGenerationBubbleController* controller =
      [[PasswordGenerationBubbleController alloc]
        initWithWindow:browser_->window()->GetNativeWindow()
            anchoredAt:point
        renderViewHost:web_contents->GetRenderViewHost()
        passwordManager:PasswordManager::FromWebContents(web_contents)
        usingGenerator:password_generator
               forForm:form];
  [controller showWindow:nil];
}
