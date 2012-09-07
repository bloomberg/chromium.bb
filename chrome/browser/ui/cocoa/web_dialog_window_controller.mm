// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_dialog_window_controller.h"

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;
using ui::WebDialogUI;
using ui::WebDialogWebContentsDelegate;

// TODO(avi): Kill this when TabContents goes away.
class WebDialogWindowControllerTabContentsCreator {
 public:
  static TabContents* CreateTabContents(content::WebContents* contents) {
    return TabContents::Factory::CreateTabContents(contents);
  }
};

// Thin bridge that routes notifications to
// WebDialogWindowController's member variables.
class WebDialogWindowDelegateBridge
    : public WebDialogDelegate,
      public WebDialogWebContentsDelegate {
public:
  // All parameters must be non-NULL/non-nil.
  WebDialogWindowDelegateBridge(WebDialogWindowController* controller,
                                content::BrowserContext* context,
                                WebDialogDelegate* delegate);

  virtual ~WebDialogWindowDelegateBridge();

  // Called when the window is directly closed, e.g. from the close
  // button or from an accelerator.
  void WindowControllerClosed();

  // WebDialogDelegate declarations.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual void GetMinimumDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE { return true; }

  // WebDialogWebContentsDelegate declarations.
  virtual void MoveContents(WebContents* source, const gfx::Rect& pos);
  virtual void HandleKeyboardEvent(content::WebContents* source,
                                   const NativeWebKeyboardEvent& event);
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;

private:
  WebDialogWindowController* controller_;  // weak
  WebDialogDelegate* delegate_;  // weak, owned by controller_

  // Calls delegate_'s OnDialogClosed() exactly once, nulling it out afterwards
  // so that no other WebDialogDelegate calls are sent to it. Returns whether or
  // not the OnDialogClosed() was actually called on the delegate.
  bool DelegateOnDialogClosed(const std::string& json_retval);

  DISALLOW_COPY_AND_ASSIGN(WebDialogWindowDelegateBridge);
};

// ChromeEventProcessingWindow expects its controller to implement the
// BrowserCommandExecutor protocol.
@interface WebDialogWindowController (InternalAPI) <BrowserCommandExecutor>

// BrowserCommandExecutor methods.
- (void)executeCommand:(int)command;

@end

namespace chrome {

gfx::NativeWindow ShowWebDialog(gfx::NativeWindow parent,
                                content::BrowserContext* context,
                                WebDialogDelegate* delegate) {
  return [WebDialogWindowController showWebDialog:delegate
                                          context:context];
}

}  // namespace chrome

WebDialogWindowDelegateBridge::WebDialogWindowDelegateBridge(
    WebDialogWindowController* controller,
    content::BrowserContext* context,
    WebDialogDelegate* delegate)
    : WebDialogWebContentsDelegate(context, new ChromeWebContentsHandler),
      controller_(controller),
      delegate_(delegate) {
  DCHECK(controller_);
  DCHECK(delegate_);
}

WebDialogWindowDelegateBridge::~WebDialogWindowDelegateBridge() {}

void WebDialogWindowDelegateBridge::WindowControllerClosed() {
  Detach();
  controller_ = nil;
  DelegateOnDialogClosed("");
}

bool WebDialogWindowDelegateBridge::DelegateOnDialogClosed(
    const std::string& json_retval) {
  if (delegate_) {
    WebDialogDelegate* real_delegate = delegate_;
    delegate_ = NULL;
    real_delegate->OnDialogClosed(json_retval);
    return true;
  }
  return false;
}

// WebDialogDelegate definitions.

// All of these functions check for NULL first since delegate_ is set
// to NULL when the window is closed.

ui::ModalType WebDialogWindowDelegateBridge::GetDialogModalType() const {
  // TODO(akalin): Support modal dialog boxes.
  if (delegate_ && delegate_->GetDialogModalType() != ui::MODAL_TYPE_NONE) {
    LOG(WARNING) << "Modal Web dialogs are not supported yet";
  }
  return ui::MODAL_TYPE_NONE;
}

string16 WebDialogWindowDelegateBridge::GetDialogTitle() const {
  return delegate_ ? delegate_->GetDialogTitle() : string16();
}

GURL WebDialogWindowDelegateBridge::GetDialogContentURL() const {
  return delegate_ ? delegate_->GetDialogContentURL() : GURL();
}

void WebDialogWindowDelegateBridge::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_) {
    delegate_->GetWebUIMessageHandlers(handlers);
  } else {
    // TODO(akalin): Add this clause in the windows version.  Also
    // make sure that everything expects handlers to be non-NULL and
    // document it.
    handlers->clear();
  }
}

void WebDialogWindowDelegateBridge::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
  else
    *size = gfx::Size();
}

void WebDialogWindowDelegateBridge::GetMinimumDialogSize(
    gfx::Size* size) const {
  if (delegate_)
    delegate_->GetMinimumDialogSize(size);
  else
    *size = gfx::Size();
}

std::string WebDialogWindowDelegateBridge::GetDialogArgs() const {
  return delegate_ ? delegate_->GetDialogArgs() : "";
}

void WebDialogWindowDelegateBridge::OnDialogClosed(
    const std::string& json_retval) {
  Detach();
  // [controller_ close] should be called at most once, too.
  if (DelegateOnDialogClosed(json_retval)) {
    [controller_ close];
  }
  controller_ = nil;
}

void WebDialogWindowDelegateBridge::OnCloseContents(WebContents* source,
                                                    bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

void WebDialogWindowDelegateBridge::CloseContents(WebContents* source) {
  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

content::WebContents* WebDialogWindowDelegateBridge::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* new_contents = NULL;
  if (delegate_ &&
      delegate_->HandleOpenURLFromTab(source, params, &new_contents)) {
    return new_contents;
  }
  return WebDialogWebContentsDelegate::OpenURLFromTab(source, params);
}

void WebDialogWindowDelegateBridge::AddNewContents(
    content::WebContents* source,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  if (delegate_ && delegate_->HandleAddNewContents(
          source, new_contents, disposition, initial_pos, user_gesture)) {
    return;
  }
  WebDialogWebContentsDelegate::AddNewContents(
      source, new_contents, disposition, initial_pos, user_gesture,
      was_blocked);
}

void WebDialogWindowDelegateBridge::LoadingStateChanged(
    content::WebContents* source) {
  if (delegate_)
    delegate_->OnLoadingStateChanged(source);
}

void WebDialogWindowDelegateBridge::MoveContents(WebContents* source,
                                                 const gfx::Rect& pos) {
  // TODO(akalin): Actually set the window bounds.
}

// A simplified version of BrowserWindowCocoa::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void WebDialogWindowDelegateBridge::HandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return;

  // Close ourselves if the user hits Esc or Command-. .  The normal
  // way to do this is to implement (void)cancel:(int)sender, but
  // since we handle keyboard events ourselves we can't do that.
  //
  // According to experiments, hitting Esc works regardless of the
  // presence of other modifiers (as long as it's not an app-level
  // shortcut, e.g. Commmand-Esc for Front Row) but no other modifiers
  // can be present for Command-. to work.
  //
  // TODO(thakis): It would be nice to get cancel: to work somehow.
  // Bug: http://code.google.com/p/chromium/issues/detail?id=32828 .
  if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
      ((event.windowsKeyCode == ui::VKEY_ESCAPE) ||
       (event.windowsKeyCode == ui::VKEY_OEM_PERIOD &&
        event.modifiers == NativeWebKeyboardEvent::MetaKey))) {
    [controller_ close];
    return;
  }

  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>([controller_ window]);
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);
  [event_window redispatchKeyEvent:event.os_event];
}

@implementation WebDialogWindowController (InternalAPI)

// This gets called whenever a chrome-specific keyboard shortcut is performed
// in the Web dialog window.  We simply swallow all those events.
- (void)executeCommand:(int)command {}

@end

@implementation WebDialogWindowController

// NOTE(akalin): We'll probably have to add the parentWindow parameter back
// in once we implement modal dialogs.

+ (NSWindow*)showWebDialog:(WebDialogDelegate*)delegate
                   context:(content::BrowserContext*)context {
  WebDialogWindowController* webDialogWindowController =
    [[WebDialogWindowController alloc] initWithDelegate:delegate
                                                context:context];
  [webDialogWindowController loadDialogContents];
  [webDialogWindowController showWindow:nil];
  return [webDialogWindowController window];
}

- (id)initWithDelegate:(WebDialogDelegate*)delegate
               context:(content::BrowserContext*)context {
  DCHECK(delegate);
  DCHECK(context);

  gfx::Size dialogSize;
  delegate->GetDialogSize(&dialogSize);
  NSRect dialogRect = NSMakeRect(0, 0, dialogSize.width(), dialogSize.height());
  NSUInteger style = NSTitledWindowMask | NSClosableWindowMask |
      NSResizableWindowMask;
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
  [window setTitle:base::SysUTF16ToNSString(delegate->GetDialogTitle())];
  [window setMinSize:dialogRect.size];
  [window center];
  delegate_.reset(
      new WebDialogWindowDelegateBridge(self, context, delegate));
  return self;
}

- (void)loadDialogContents {
  tabContents_.reset(
      WebDialogWindowControllerTabContentsCreator::CreateTabContents(
          WebContents::Create(
              delegate_->browser_context(), NULL, MSG_ROUTING_NONE, NULL)));
  [[self window]
      setContentView:tabContents_->web_contents()->GetNativeView()];
  tabContents_->web_contents()->SetDelegate(delegate_.get());

  // This must be done before loading the page; see the comments in
  // WebDialogUI.
  WebDialogUI::SetDelegate(tabContents_->web_contents(), delegate_.get());

  tabContents_->web_contents()->GetController().LoadURL(
      delegate_->GetDialogContentURL(),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

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
