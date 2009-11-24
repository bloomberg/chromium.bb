// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/html_dialog_window_controller.h"

#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/browser_command_executor.h"
#import "chrome/browser/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "googleurl/src/gurl.h"

// Thin bridge that routes notifications to
// HtmlDialogWindowController's member variables.
class HtmlDialogWindowDelegateBridge : public HtmlDialogUIDelegate,
                                       public TabContentsDelegate {
public:
  // All parameters must be non-NULL/non-nil.
  HtmlDialogWindowDelegateBridge(HtmlDialogWindowController* controller,
                                 HtmlDialogUIDelegate* delegate);

  virtual ~HtmlDialogWindowDelegateBridge();

  // Called when the window is directly closed, e.g. from the close
  // button or from an accelerator.
  void WindowControllerClosed();

  // HtmlDialogUIDelegate declarations.
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

  // TabContentsDelegate declarations.
  //
  // TODO(akalin): decomp this out into a separate platform-independent class
  // so they don't diverge in behavior.  In particular,
  // ShouldAddNavigationToHistory() should return false on all platforms but
  // currently does so only on OS X.  This is tracked in
  // http://code.google.com/p/chromium/issues/detail?id=28609 .
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual bool ShouldAddNavigationToHistory() const;

private:
  HtmlDialogWindowController* controller_;  // weak
  HtmlDialogUIDelegate* delegate_;  // weak, owned by controller_

  // Calls delegate_'s OnDialogClosed() exactly once, nulling it out
  // afterwards so that no other HtmlDialogUIDelegate calls are sent
  // to it.  Returns whether or not the OnDialogClosed() was actually
  // called on the delegate.
  bool DelegateOnDialogClosed(const std::string& json_retval);

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogWindowDelegateBridge);
};

// ChromeEventProcessingWindow expects its controller to implement the
// BrowserCommandExecutor protocol.
@interface HtmlDialogWindowController (InternalAPI) <BrowserCommandExecutor>

- (Profile*)profile;

// BrowserCommandExecutor methods.
- (void)executeCommand:(int)command;

@end

namespace html_dialog_window_controller {

void ShowHtmlDialog(HtmlDialogUIDelegate* delegate, Profile* profile) {
  [HtmlDialogWindowController showHtmlDialog:delegate profile:profile];
}

}  // namespace html_dialog_window_controller

HtmlDialogWindowDelegateBridge::HtmlDialogWindowDelegateBridge(
    HtmlDialogWindowController* controller, HtmlDialogUIDelegate* delegate)
    : controller_(controller), delegate_(delegate) {
  DCHECK(controller_);
  DCHECK(delegate_);
}

HtmlDialogWindowDelegateBridge::~HtmlDialogWindowDelegateBridge() {}

void HtmlDialogWindowDelegateBridge::WindowControllerClosed() {
  DelegateOnDialogClosed("");
  controller_ = nil;
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
  controller_ = nil;
}

// TabContentsDelegate definitions.  Most of this logic is copied from
// chrome/browser/views/html_dialog_view.cc .  All functions with empty
// bodies are notifications we don't care about.

void HtmlDialogWindowDelegateBridge::OpenURLFromTab(
    TabContents* source, const GURL& url, const GURL& referrer,
    WindowOpenDisposition disposition, PageTransition::Type transition) {
  if (controller_) {
    // Force all links to open in a new window.  Code adapted from
    // Browser::OpenURLFromTab() with disposition == NEW_WINDOW.
    Browser* browser = Browser::Create([controller_ profile]);
    TabContents* new_contents =
      browser->AddTabWithURL(url, referrer, transition, true, -1, false, NULL);
    new_contents->Focus();
  }
}

void HtmlDialogWindowDelegateBridge::NavigationStateChanged(
  const TabContents* source, unsigned changed_flags) {
}

void HtmlDialogWindowDelegateBridge::AddNewContents(
    TabContents* source, TabContents* new_contents,
    WindowOpenDisposition disposition, const gfx::Rect& initial_pos,
    bool user_gesture) {
  if (controller_) {
    // Force this to open in a new window, too.  Code adapted from
    // Browser::AddNewContents() with disposition == NEW_WINDOW.
    Browser* browser = Browser::Create([controller_ profile]);
    static_cast<TabContentsDelegate*>(browser)->
      AddNewContents(source, new_contents, NEW_FOREGROUND_TAB,
                     initial_pos, user_gesture);
    browser->window()->Show();
  }
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

bool HtmlDialogWindowDelegateBridge::ShouldAddNavigationToHistory() const {
  return false;
}

@implementation HtmlDialogWindowController (InternalAPI)

- (Profile*)profile {
  return profile_;
}

// This gets called whenever a chrome-specific keyboard shortcut is performed
// in the HTML dialog window.  We simply swallow all those events.
- (void)executeCommand:(int)command {}

@end

@implementation HtmlDialogWindowController

// NOTE(akalin): We'll probably have to add the parentWindow parameter back
// in once we implement modal dialogs.

+ (void)showHtmlDialog:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile {
  HtmlDialogWindowController* htmlDialogWindowController =
    [[HtmlDialogWindowController alloc] initWithDelegate:delegate
                                                 profile:profile];
  [htmlDialogWindowController loadDialogContents];
  [htmlDialogWindowController showWindow:nil];
}

- (id)initWithDelegate:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile {
  DCHECK(delegate);
  DCHECK(profile);

  gfx::Size dialogSize;
  delegate->GetDialogSize(&dialogSize);
  NSRect dialogRect = NSMakeRect(0, 0, dialogSize.width(), dialogSize.height());
  // TODO(akalin): Make the window resizable (but with the minimum size being
  // dialog_size and always on top (but not modal) to match the Windows
  // behavior.  On the other hand, the fact that HTML dialogs on Windows
  // are resizable could just be an accident.  Investigate futher...
  NSUInteger style = NSTitledWindowMask | NSClosableWindowMask;
  scoped_nsobject<ChromeEventProcessingWindow> window(
      [[ChromeEventProcessingWindow alloc]
           initWithContentRect:dialogRect
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
  [window center];
  delegate_.reset(new HtmlDialogWindowDelegateBridge(self, delegate));
  // Incognito profiles are not long-lived, so we always want to store a
  // non-incognito profile.
  //
  // TODO(akalin): Should we make it so that we have a default incognito
  // profile that's long-lived?  Of course, we'd still have to clear it out
  // when all incognito browsers close.
  profile_ = profile->GetOriginalProfile();
  return self;
}

- (void)loadDialogContents {
  tabContents_.reset(new TabContents(profile_, NULL, MSG_ROUTING_NONE, NULL));
  [[self window] setContentView:tabContents_->GetNativeView()];
  tabContents_->set_delegate(delegate_.get());

  // This must be done before loading the page; see the comments in
  // HtmlDialogUI.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tabContents_->property_bag(),
                                                  delegate_.get());

  tabContents_->controller().LoadURL(delegate_->GetDialogContentURL(),
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

