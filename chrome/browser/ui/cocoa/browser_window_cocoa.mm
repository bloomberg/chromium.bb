// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"
#import "chrome/browser/ui/cocoa/browser/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/web_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/translate/core/browser/language_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"
#import "chrome/browser/ui/cocoa/one_click_signin_dialog_controller.h"
#endif

using content::NativeWebKeyboardEvent;
using content::WebContents;

namespace {

// These UI constants are used in BrowserWindowCocoa::ShowBookmarkAppBubble.
// Used for defining the layout of the NSAlert and NSTextField within the
// accessory view.
const int kAppTextFieldVerticalSpacing = 2;
const int kAppTextFieldWidth = 200;
const int kAppTextFieldHeight = 22;
const int kBookmarkAppBubbleViewWidth = 200;
const int kBookmarkAppBubbleViewHeight = 46;

const int kIconPreviewTargetSize = 128;

base::string16 TrimText(NSString* controlText) {
  base::string16 text = base::SysNSStringToUTF16(controlText);
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

}  // namespace

@interface TextRequiringDelegate : NSObject<NSTextFieldDelegate> {
 @private
  // Disable |control_| when text changes to just whitespace or empty string.
  NSControl* control_;
}
- (id)initWithControl:(NSControl*)control text:(NSString*)text;
- (void)controlTextDidChange:(NSNotification*)notification;
@end

@interface TextRequiringDelegate ()
- (void)validateText:(NSString*)text;
@end

@implementation TextRequiringDelegate
- (id)initWithControl:(NSControl*)control text:(NSString*)text {
  if ((self = [super init])) {
    control_ = control;
    [self validateText:text];
  }
  return self;
}

- (void)controlTextDidChange:(NSNotification*)notification {
  [self validateText:[[notification object] stringValue]];
}

- (void)validateText:(NSString*)text {
  [control_ setEnabled:TrimText(text).empty() ? NO : YES];
}
@end

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller)
  : browser_(browser),
    controller_(controller),
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
  // Turn off swishing when restoring windows or showing an app.
  if ((is_session_restore || browser_->is_app()) &&
      [window() respondsToSelector:@selector(animationBehavior)] &&
      [window() respondsToSelector:@selector(setAnimationBehavior:)]) {
    did_save_animation_behavior = true;
    saved_animation_behavior = [window() animationBehavior];
    [window() setAnimationBehavior:NSWindowAnimationBehaviorNone];
  }

  {
    TRACE_EVENT0("ui", "BrowserWindowCocoa::Show makeKeyAndOrderFront");
    // This call takes up a substantial part of startup time, and an even more
    // substantial part of startup time when any CALayers are part of the
    // window's NSView heirarchy.
    [window() makeKeyAndOrderFront:controller_];
  }

  // When creating windows from nibs it is necessary to |makeKeyAndOrderFront:|
  // prior to |orderOut:| then |miniaturize:| when restoring windows in the
  // minimized state.
  if (initial_show_state_ == ui::SHOW_STATE_MINIMIZED) {
    [window() orderOut:controller_];
    [window() miniaturize:controller_];
  } else if (initial_show_state_ == ui::SHOW_STATE_FULLSCREEN) {
    chrome::ToggleFullscreenWithToolbarOrFallback(browser_);
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
  [window() orderOut:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect real_bounds = [controller_ enforceMinWindowSize:bounds];

  ExitFullscreen();
  NSRect cocoa_bounds = NSMakeRect(real_bounds.x(), 0,
                                   real_bounds.width(),
                                   real_bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] firstObject];
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

void BrowserWindowCocoa::SetAlwaysOnTop(bool always_on_top) {
  // Not implemented for browser windows.
  NOTIMPLEMENTED();
}

bool BrowserWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeWindow() const {
  return window();
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return [controller_ statusBubble];
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NSString* newTitle = WindowTitle();

  pending_window_title_.reset([BrowserWindowUtils
      scheduleReplaceOldTitle:pending_window_title_.get()
                 withNewTitle:newTitle
                    forWindow:window()]);
}

NSString* BrowserWindowCocoa::WindowTitle() {
  if (media_state_ == TAB_MEDIA_STATE_AUDIO_PLAYING) {
    return l10n_util::GetNSStringF(IDS_WINDOW_AUDIO_PLAYING_MAC,
                                   browser_->GetWindowTitleForCurrentTab(),
                                   base::SysNSStringToUTF16(@"🔊"));
  } else if (media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING) {
    return l10n_util::GetNSStringF(IDS_WINDOW_AUDIO_MUTING_MAC,
                                   browser_->GetWindowTitleForCurrentTab(),
                                   base::SysNSStringToUTF16(@"🔇"));
  }
  return base::SysUTF16ToNSString(browser_->GetWindowTitleForCurrentTab());
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
  [controller_ setStarredState:is_starred];
}

void BrowserWindowCocoa::SetTranslateIconToggled(bool is_lit) {
  [controller_ setCurrentPageIsTranslated:is_lit];
}

void BrowserWindowCocoa::OnActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  [controller_ onActiveTabChanged:old_contents to:new_contents];
  // TODO(pkasting): Perhaps the code in
  // TabStripController::activateTabWithContents should move here?  Or this
  // should call that (instead of TabStripModelObserverBridge doing so)?  It's
  // not obvious to me why Mac doesn't handle tab changes in BrowserWindow the
  // way views and GTK do.
  // See http://crbug.com/340720 for discussion.
}

void BrowserWindowCocoa::ZoomChangedForActiveTab(bool can_show_bubble) {
  [controller_ zoomChangedForActiveTab:can_show_bubble ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] firstObject];
  NSRect frame = [controller_ regularWindowFrame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

ui::WindowShowState BrowserWindowCocoa::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsMinimized())
    return ui::SHOW_STATE_MINIMIZED;
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect BrowserWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

bool BrowserWindowCocoa::IsMaximized() const {
  // -isZoomed returns YES if the window's frame equals the rect returned by
  // -windowWillUseStandardFrame:defaultFrame:, even if the window is in the
  // dock, so have to explicitly check for miniaturization state first.
  return ![window() isMiniaturized] && [window() isZoomed];
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

// See browser_window_controller.h for a detailed explanation of the logic in
// this method.
void BrowserWindowCocoa::EnterFullscreen(const GURL& url,
                                         ExclusiveAccessBubbleType bubble_type,
                                         bool with_toolbar) {
  if (browser_->exclusive_access_manager()
          ->fullscreen_controller()
          ->IsWindowFullscreenForTabOrPending())
    [controller_ enterWebContentFullscreenForURL:url bubbleType:bubble_type];
  else if (!url.is_empty())
    [controller_ enterExtensionFullscreenForURL:url bubbleType:bubble_type];
  else
    [controller_ enterBrowserFullscreenWithToolbar:with_toolbar];
}

void BrowserWindowCocoa::ExitFullscreen() {
  [controller_ exitAnyFullscreen];
}

void BrowserWindowCocoa::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  [controller_ updateFullscreenExitBubbleURL:url bubbleType:bubble_type];
}

void BrowserWindowCocoa::OnExclusiveAccessUserInput() {
  // TODO(mgiuca): Route this signal to the exclusive access bubble on Mac.
}

bool BrowserWindowCocoa::ShouldHideUIForFullscreen() const {
  // On Mac, fullscreen mode has most normal things (in a slide-down panel).
  return false;
}

bool BrowserWindowCocoa::IsFullscreen() const {
  return [controller_ isInAnyFullscreenMode];
}

bool BrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
}

bool BrowserWindowCocoa::SupportsFullscreenWithToolbar() const {
  return chrome::mac::SupportsSystemFullscreen();
}

void BrowserWindowCocoa::UpdateFullscreenWithToolbar(bool with_toolbar) {
  [controller_ updateFullscreenWithToolbar:with_toolbar];
}

void BrowserWindowCocoa::ToggleFullscreenToolbar() {
  PrefService* prefs = browser_->profile()->GetPrefs();
  bool hideToolbar = !prefs->GetBoolean(prefs::kHideFullscreenToolbar);
  [controller_ setFullscreenToolbarHidden:hideToolbar];
  prefs->SetBoolean(prefs::kHideFullscreenToolbar, hideToolbar);
}

bool BrowserWindowCocoa::IsFullscreenWithToolbar() const {
  return IsFullscreen() && ![controller_ inPresentationMode];
}

bool BrowserWindowCocoa::ShouldHideFullscreenToolbar() const {
  return [controller_ shouldHideFullscreenToolbar];
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

void BrowserWindowCocoa::UpdateToolbar(content::WebContents* contents) {
  [controller_ updateToolbarWithContents:contents];
}

void BrowserWindowCocoa::ResetToolbarTabState(content::WebContents* contents) {
  [controller_ resetTabState:contents];
}

void BrowserWindowCocoa::FocusToolbar() {
  // Not needed on the Mac.
}

ToolbarActionsBar* BrowserWindowCocoa::GetToolbarActionsBar() {
  if ([controller_ hasToolbar])
    return [[[controller_ toolbarController] browserActionsController]
               toolbarActionsBar];
  return nullptr;
}

void BrowserWindowCocoa::ToolbarSizeChanged(bool is_animating) {
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

void BrowserWindowCocoa::FocusInfobars() {
  // Not needed on the Mac.
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  return browser_->profile()->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowBookmarkBar);
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

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  [controller_ addFindBar:find_bar_cocoa_controller];
}

void BrowserWindowCocoa::UpdateMediaState(TabMediaState media_state) {
  media_state_ = media_state;
  UpdateTitleBar();
}

void BrowserWindowCocoa::ShowUpdateChromeDialog() {
  restart_browser::RequestRestart(window());
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  [controller_ showBookmarkBubbleForURL:url
                      alreadyBookmarked:(already_bookmarked ? YES : NO)];
}

void BrowserWindowCocoa::ShowBookmarkAppBubble(
    const WebApplicationInfo& web_app_info,
    const ShowBookmarkAppBubbleCallback& callback) {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:l10n_util::GetNSString(
      IDS_ADD_TO_APPLICATIONS_BUBBLE_TITLE)];
  [alert setAlertStyle:NSInformationalAlertStyle];

  NSButton* continue_button =
      [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  [continue_button setKeyEquivalent:kKeyEquivalentReturn];
  NSButton* cancel_button =
      [alert addButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)];
  [cancel_button setKeyEquivalent:kKeyEquivalentEscape];

  base::scoped_nsobject<NSButton> open_as_window_checkbox(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [open_as_window_checkbox setButtonType:NSSwitchButton];
  [open_as_window_checkbox
      setTitle:l10n_util::GetNSString(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW)];
  [open_as_window_checkbox setState:web_app_info.open_as_window];
  [open_as_window_checkbox sizeToFit];

  base::scoped_nsobject<NSTextField> app_title([[NSTextField alloc]
      initWithFrame:NSMakeRect(0, kAppTextFieldHeight +
                                      kAppTextFieldVerticalSpacing,
                               kAppTextFieldWidth, kAppTextFieldHeight)]);
  NSString* original_title = SysUTF16ToNSString(web_app_info.title);
  [[app_title cell] setWraps:NO];
  [[app_title cell] setScrollable:YES];
  [app_title setStringValue:original_title];
  base::scoped_nsobject<TextRequiringDelegate> delegate(
      [[TextRequiringDelegate alloc] initWithControl:continue_button
                                                text:[app_title stringValue]]);
  [app_title setDelegate:delegate];

  base::scoped_nsobject<NSView> view([[NSView alloc]
      initWithFrame:NSMakeRect(0, 0, kBookmarkAppBubbleViewWidth,
                               kBookmarkAppBubbleViewHeight)]);

  // When CanHostedAppsOpenInWindows() returns false, do not show the open as
  // window checkbox to avoid confusing users.
  if (extensions::util::CanHostedAppsOpenInWindows())
    [view addSubview:open_as_window_checkbox];
  [view addSubview:app_title];
  [alert setAccessoryView:view];

  // Find the image with target size.
  // Assumes that the icons are sorted in ascending order of size.
  if (!web_app_info.icons.empty()) {
    for (const WebApplicationInfo::IconInfo& info : web_app_info.icons) {
      if (info.width == kIconPreviewTargetSize &&
          info.height == kIconPreviewTargetSize) {
        gfx::Image icon_image = gfx::Image::CreateFrom1xBitmap(info.data);
        [alert setIcon:icon_image.ToNSImage()];
        break;
      }
    }
  }

  NSInteger response = [alert runModal];

  // Prevent |app_title| from accessing |delegate| after it's destroyed.
  [app_title setDelegate:nil];

  if (response == NSAlertFirstButtonReturn) {
    WebApplicationInfo updated_info = web_app_info;
    updated_info.open_as_window = [open_as_window_checkbox state] == NSOnState;
    updated_info.title = TrimText([app_title stringValue]);

    callback.Run(true, updated_info);
    return;
  }

  callback.Run(false, web_app_info);
}

autofill::SaveCardBubbleView* BrowserWindowCocoa::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    bool user_gesture) {
  return new autofill::SaveCardBubbleViewBridge(controller, controller_);
}

void BrowserWindowCocoa::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  translate::LanguageState& language_state =
      chrome_translate_client->GetLanguageState();
  language_state.SetTranslateEnabled(true);

  [controller_ showTranslateBubbleForWebContents:contents
                                            step:step
                                       errorType:error_type];
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void BrowserWindowCocoa::ShowOneClickSigninBubble(
    OneClickSigninBubbleType type,
    const base::string16& email,
    const base::string16& error_message,
    const StartSyncCallback& start_sync_callback) {
  WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
  if (type == ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE) {
    base::scoped_nsobject<OneClickSigninBubbleController> bubble_controller([
            [OneClickSigninBubbleController alloc]
        initWithBrowserWindowController:cocoa_controller()
                            webContents:web_contents
                           errorMessage:base::SysUTF16ToNSString(error_message)
                               callback:start_sync_callback]);
    [bubble_controller showWindow:nil];
  } else {
    // Deletes itself when the dialog closes.
    new OneClickSigninDialogController(
        web_contents, start_sync_callback, email);
  }
}
#endif

bool BrowserWindowCocoa::IsDownloadShelfVisible() const {
  return [controller_ isDownloadShelfVisible] != NO;
}

DownloadShelf* BrowserWindowCocoa::GetDownloadShelf() {
  [controller_ createAndAddDownloadShelf];
  DownloadShelfController* shelfController = [controller_ downloadShelf];
  return [shelfController bridge];
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

void BrowserWindowCocoa::UserChangedTheme() {
  [controller_ userChangedTheme];
}

void BrowserWindowCocoa::ShowWebsiteSettings(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityStateModel::SecurityInfo& security_info) {
  WebsiteSettingsUIBridge::Show(window(), profile, web_contents, url,
                                security_info);
}

void BrowserWindowCocoa::ShowAppMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

bool BrowserWindowCocoa::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  // Handle ESC to dismiss permission bubbles, but still forward it
  // to the window afterwards.
  if (event.windowsKeyCode == ui::VKEY_ESCAPE)
    [controller_ dismissPermissionBubble];

  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return false;

  if (event.type == blink::WebInputEvent::RawKeyDown &&
      [controller_
          handledByExtensionCommand:event.os_event
                           priority:ui::AcceleratorManager::kHighPriority])
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

void BrowserWindowCocoa::CutCopyPaste(int command_id) {
  if (command_id == IDC_CUT)
    [NSApp sendAction:@selector(cut:) to:nil from:nil];
  else if (command_id == IDC_COPY)
    [NSApp sendAction:@selector(copy:) to:nil from:nil];
  else
    [NSApp sendAction:@selector(paste:) to:nil from:nil];
}

WindowOpenDisposition BrowserWindowCocoa::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  // When using Cocoa's System Fullscreen mode, convert popups into tabs.
  if ([controller_ isInAppKitFullscreen])
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

web_modal::WebContentsModalDialogHost*
    BrowserWindowCocoa::GetWebContentsModalDialogHost() {
  DCHECK([controller_ window]);
  ConstrainedWindowSheetController* sheet_controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:[controller_ window]];
  DCHECK(sheet_controller);
  return [sheet_controller dialogHost];
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
}

void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}

NSWindow* BrowserWindowCocoa::window() const {
  return [controller_ window];
}

void BrowserWindowCocoa::ShowAvatarBubbleFromAvatarButton(
    AvatarBubbleMode mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point) {
  AvatarBaseController* controller = [controller_ avatarButtonController];
  NSView* anchor = [controller buttonView];
  if ([anchor isHiddenOrHasHiddenAncestor])
    anchor = [[controller_ toolbarController] appMenuButton];
  [controller showAvatarBubbleAnchoredAt:anchor
                                withMode:mode
                         withServiceType:manage_accounts_params.service_type
                         fromAccessPoint:access_point];
}

void BrowserWindowCocoa::ShowModalSigninWindow(
    AvatarBubbleMode mode,
    signin_metrics::AccessPoint access_point) {
  NOTREACHED();
}
void BrowserWindowCocoa::CloseModalSigninWindow() {
  NOTREACHED();
}

int
BrowserWindowCocoa::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  if (browser_->bookmark_bar_state() != BookmarkBar::DETACHED)
    return 0;
  return 40;
}

void BrowserWindowCocoa::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {
  [cocoa_controller() executeExtensionCommand:extension->id() command:command];
}

ExclusiveAccessContext* BrowserWindowCocoa::GetExclusiveAccessContext() {
  return this;
}

Profile* BrowserWindowCocoa::GetProfile() {
  return browser_->profile();
}

WebContents* BrowserWindowCocoa::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserWindowCocoa::UnhideDownloadShelf() {
  GetDownloadShelf()->Unhide();
}

void BrowserWindowCocoa::HideDownloadShelf() {
  GetDownloadShelf()->Hide();
  StatusBubble* statusBubble = GetStatusBubble();
  if (statusBubble)
    statusBubble->Hide();
}
