// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/html_dialog_window_controller.h"

#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/browser_command_executor.h"
#import "chrome/browser/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"

// Thin bridge that routes notifications to
// HtmlDialogWindowController's member variables.
class HtmlDialogWindowDelegateBridge : public HtmlDialogUIDelegate,
                                       public HtmlDialogTabContentsDelegate {
public:
  // All parameters must be non-NULL/non-nil.
  HtmlDialogWindowDelegateBridge(HtmlDialogWindowController* controller,
                                 Profile* profile,
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

  // HtmlDialogTabContentsDelegate declarations.
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

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

// BrowserCommandExecutor methods.
- (void)executeCommand:(int)command;

@end

namespace html_dialog_window_controller {

void ShowHtmlDialog(HtmlDialogUIDelegate* delegate, Profile* profile) {
  [HtmlDialogWindowController showHtmlDialog:delegate profile:profile];
}

}  // namespace html_dialog_window_controller

HtmlDialogWindowDelegateBridge::HtmlDialogWindowDelegateBridge(
    HtmlDialogWindowController* controller, Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      controller_(controller), delegate_(delegate) {
  DCHECK(controller_);
  DCHECK(delegate_);
}

HtmlDialogWindowDelegateBridge::~HtmlDialogWindowDelegateBridge() {}

void HtmlDialogWindowDelegateBridge::WindowControllerClosed() {
  Detach();
  controller_ = nil;
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
  Detach();
  // [controller_ close] should be called at most once, too.
  if (DelegateOnDialogClosed(json_retval)) {
    [controller_ close];
  }
  controller_ = nil;
}

void HtmlDialogWindowDelegateBridge::MoveContents(TabContents* source,
                                                  const gfx::Rect& pos) {
  // TODO(akalin): Actually set the window bounds.
}

void HtmlDialogWindowDelegateBridge::ToolbarSizeChanged(
    TabContents* source, bool is_animating) {
  // TODO(akalin): Figure out what to do here.
}

// A simplified version of BrowserWindowCocoa::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void HtmlDialogWindowDelegateBridge::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return;

  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>([controller_ window]);

  [event_window redispatchEvent:event.os_event];
}

@implementation HtmlDialogWindowController (InternalAPI)

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
  delegate_.reset(new HtmlDialogWindowDelegateBridge(self, profile, delegate));
  return self;
}

- (void)loadDialogContents {
  tabContents_.reset(
      new TabContents(delegate_->profile(), NULL, MSG_ROUTING_NONE, NULL));
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

