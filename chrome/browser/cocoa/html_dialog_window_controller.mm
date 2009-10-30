// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/html_dialog_window_controller.h"

#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_command_executor.h"
#import "chrome/browser/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

HtmlDialogWindowDelegateBridge::HtmlDialogWindowDelegateBridge(
    HtmlDialogUIDelegate* delegate, NSWindowController* controller,
    NSWindow* window, Browser* browser)
    : delegate_(delegate), controller_(controller), window_(window),
      browser_(browser) {
  DCHECK(delegate_);
  DCHECK(controller_);
  DCHECK(window_);
  DCHECK(browser_);
}

HtmlDialogWindowDelegateBridge::~HtmlDialogWindowDelegateBridge() {}

void HtmlDialogWindowDelegateBridge::WindowControllerClosed() {
  DelegateOnDialogClosed("");
}

bool HtmlDialogWindowDelegateBridge::DelegateOnDialogClosed(
    const std::string& json_retval) {
  if (delegate_) {
    HtmlDialogUIDelegate* real_delegate = delegate_;
    delegate_ = NULL;
    real_delegate->OnDialogClosed(json_retval);
    return true;
  }
  return false;
}

// HtmlDialogUIDelegate definitions.

// All of these functions check for NULL first since delegate_ is set
// to NULL when the window is closed.

bool HtmlDialogWindowDelegateBridge::IsDialogModal() const {
  // TODO(akalin): Support modal dialog boxes.
  if (delegate_ && delegate_->IsDialogModal()) {
    LOG(WARNING) << "Modal HTML dialogs are not supported yet";
  }
  return false;
}

std::wstring HtmlDialogWindowDelegateBridge::GetDialogTitle() const {
  return delegate_ ? delegate_->GetDialogTitle() : L"";
}

GURL HtmlDialogWindowDelegateBridge::GetDialogContentURL() const {
  return delegate_ ? delegate_->GetDialogContentURL() : GURL();
}

void HtmlDialogWindowDelegateBridge::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  if (delegate_) {
    delegate_->GetDOMMessageHandlers(handlers);
  } else {
    // TODO(akalin): Add this clause in the windows version.  Also
    // make sure that everything expects handlers to be non-NULL and
    // document it.
    handlers->clear();
  }
}

void HtmlDialogWindowDelegateBridge::GetDialogSize(gfx::Size* size) const {
  if (delegate_) {
    delegate_->GetDialogSize(size);
  } else {
    *size = gfx::Size();
  }
}

std::string HtmlDialogWindowDelegateBridge::GetDialogArgs() const {
  return delegate_ ? delegate_->GetDialogArgs() : "";
}

void HtmlDialogWindowDelegateBridge::OnDialogClosed(
    const std::string& json_retval) {
  // [controller_ close] should be called at most once, too.
  if (DelegateOnDialogClosed(json_retval)) {
    [controller_ close];
  }
}

// TabContentsDelegate definitions.  Most of this logic is copied from
// chrome/browser/views/html_dialog_view.cc .  All functions with empty
// bodies are notifications we don't care about.

void HtmlDialogWindowDelegateBridge::OpenURLFromTab(
    TabContents* source, const GURL& url, const GURL& referrer,
    WindowOpenDisposition disposition, PageTransition::Type transition) {
  // Force all links to open in a new window.
  static_cast<TabContentsDelegate*>(browser_)->
    OpenURLFromTab(source, url, referrer, NEW_WINDOW, transition);
}

void HtmlDialogWindowDelegateBridge::NavigationStateChanged(
  const TabContents* source, unsigned changed_flags) {
}

void HtmlDialogWindowDelegateBridge::AddNewContents(
    TabContents* source, TabContents* new_contents,
    WindowOpenDisposition disposition, const gfx::Rect& initial_pos,
    bool user_gesture) {
  // Force this to open in a new window, too.
  static_cast<TabContentsDelegate*>(browser_)->
    AddNewContents(source, new_contents, NEW_WINDOW,
                   initial_pos, user_gesture);
}

void HtmlDialogWindowDelegateBridge::ActivateContents(TabContents* contents) {}

void HtmlDialogWindowDelegateBridge::LoadingStateChanged(TabContents* source) {}

void HtmlDialogWindowDelegateBridge::CloseContents(TabContents* source) {}

void HtmlDialogWindowDelegateBridge::MoveContents(TabContents* source,
                                                  const gfx::Rect& pos) {
  // TODO(akalin): Actually set the window bounds.
}

bool HtmlDialogWindowDelegateBridge::IsPopup(TabContents* source) {
  // This needs to return true so that we are allowed to be resized by
  // our contents.
  return true;
}

void HtmlDialogWindowDelegateBridge::ToolbarSizeChanged(
    TabContents* source, bool is_animating) {
  // TODO(akalin): Figure out what to do here.
}

void HtmlDialogWindowDelegateBridge::URLStarredChanged(
    TabContents* source, bool starred) {
  // We don't have a visible star to click in the window.
  NOTREACHED();
}

void HtmlDialogWindowDelegateBridge::UpdateTargetURL(
    TabContents* source, const GURL& url) {}

// ChromeEventProcessingWindow expect its controller to implement this
// protocol.

@interface HtmlDialogWindowController (InternalAPI) <BrowserCommandExecutor>

- (void)executeCommand:(int)command;

@end

@implementation HtmlDialogWindowController (InternalAPI)

- (void)executeCommand:(int)command {
  if (browser_->command_updater()->IsCommandEnabled(command)) {
    browser_->ExecuteCommand(command);
  }
}

@end

@implementation HtmlDialogWindowController

+ (void)showHtmlDialog:(HtmlDialogUIDelegate*)delegate
          parentWindow:(gfx::NativeWindow)parent_window
               browser:(Browser*)browser {
  HtmlDialogWindowController* html_dialog_window_controller =
    [[HtmlDialogWindowController alloc] initWithDelegate:delegate
                                            parentWindow:parent_window
                                                 browser:browser];
  [html_dialog_window_controller loadDialogContents];
  [html_dialog_window_controller showWindow:nil];
}

- (id)initWithDelegate:(HtmlDialogUIDelegate*)delegate
          parentWindow:(gfx::NativeWindow)parent_window
               browser:(Browser*)browser {
  DCHECK(delegate);
  DCHECK(parent_window);
  DCHECK(browser);

  // Put the dialog box in the center of the window.
  //
  // TODO(akalin): Surely there must be a cleaner way to do this.
  //
  // TODO(akalin): Perhaps use [window center] instead, which centers
  // the dialog to the screen, although it doesn't match the Windows
  // behavior.
  NSRect parent_window_frame = [parent_window frame];
  NSPoint parent_window_origin = parent_window_frame.origin;
  NSSize parent_window_size = parent_window_frame.size;
  gfx::Size dialog_size;
  delegate->GetDialogSize(&dialog_size);
  NSRect dialog_rect =
    NSMakeRect(parent_window_origin.x +
               (parent_window_size.width - dialog_size.width()) / 2,
               parent_window_origin.y +
               (parent_window_size.height - dialog_size.height()) / 2,
               dialog_size.width(),
               dialog_size.height());
  // TODO(akalin): Make the window resizable (but with the minimum size being
  // dialog_size and always on top (but not modal) to match the Windows
  // behavior.
  NSUInteger style = NSTitledWindowMask | NSClosableWindowMask;
  scoped_nsobject<ChromeEventProcessingWindow> window(
      [[ChromeEventProcessingWindow alloc]
           initWithContentRect:dialog_rect
                     styleMask:style
                       backing:NSBackingStoreBuffered
                         defer:YES]);
  if (!window.get()) {
    return nil;
  }
  self = [super initWithWindow:window];
  if (!self) {
    return nil;
  }
  [window setWindowController:self];
  [window setDelegate:self];
  [window setTitle:base::SysWideToNSString(delegate->GetDialogTitle())];
  browser_ = browser;
  delegate_.reset(
      new HtmlDialogWindowDelegateBridge(delegate, self, window, browser));
  return self;
}

- (void)loadDialogContents {
  // TODO(akalin): Figure out if this can be an incognito profile.
  Profile* profile = browser_->profile();
  tab_contents_.reset(new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL));
  [[self window] setContentView:tab_contents_->GetNativeView()];
  tab_contents_->set_delegate(delegate_.get());

  // This must be done before loading the page; see the comments in
  // HtmlDialogUI.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tab_contents_->property_bag(),
                                                  delegate_.get());

  tab_contents_->controller().LoadURL(delegate_->GetDialogContentURL(),
                                      GURL(), PageTransition::START_PAGE);

  // TODO(akalin): add accelerator for ESC to close the dialog box.
  //
  // TODO(akalin): Figure out why implementing (void)cancel:(id)sender
  // to do the above doesn't work.
}

- (void)windowWillClose:(NSNotification*)notification {
  delegate_->WindowControllerClosed();
  [self autorelease];
}

@end

