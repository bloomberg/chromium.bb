// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"
#import "chrome/browser/ui/login/login_prompt_mac.h"

#include "base/mac/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#include "chrome/browser/ui/login/login_model.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"

using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerMac

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerMac : public LoginHandler,
                        public ConstrainedWindowMacDelegateCustomSheet {
 public:
  LoginHandlerMac(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request),
        sheet_controller_(nil) {
  }

  virtual ~LoginHandlerMac() {
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    [sheet_controller_ autofillLogin:base::SysWideToNSString(username)
                            password:base::SysWideToNSString(password)];
  }

  // LoginHandler:
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           const string16& explanation) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // Load nib here instead of in constructor.
    sheet_controller_ = [[[LoginHandlerSheet alloc]
        initWithLoginHandler:this] autorelease];
    init([sheet_controller_ window], sheet_controller_,
          @selector(sheetDidEnd:returnCode:contextInfo:));

    SetModel(manager);

    [sheet_controller_ setExplanation:base::SysUTF16ToNSString(explanation)];

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    SetDialog(GetTabContentsForLogin()->CreateConstrainedDialog(this));

    NotifyAuthNeeded();
  }

  // Overridden from ConstrainedWindowMacDelegate:
  virtual void DeleteDelegate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // The constrained window is going to delete itself; clear our pointer.
    SetDialog(NULL);
    SetModel(NULL);

    // Close sheet if it's still open, as required by
    // ConstrainedWindowMacDelegate.
    if (is_sheet_open())
      [NSApp endSheet:sheet()];

    ReleaseSoon();
  }

  void OnLoginPressed(const string16& username,
                      const string16& password) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    SetAuth(username, password);
  }

  void OnCancelPressed() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    CancelAuth();
  }

 private:
  friend class LoginPrompt;

  // The Cocoa controller of the GUI.
  LoginHandlerSheet* sheet_controller_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerMac);
};

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerMac(auth_info, request);
}

// ----------------------------------------------------------------------------
// LoginHandlerSheet

@implementation LoginHandlerSheet

- (id)initWithLoginHandler:(LoginHandlerMac*)handler {
  NSString* nibPath =
      [base::mac::MainAppBundle() pathForResource:@"HttpAuthLoginSheet"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath
                                     owner:self])) {
    handler_ = handler;
  }
  return self;
}

- (void)dealloc {
  // The buttons could be in a modal loop, so disconnect them so they cannot
  // call back to us after we're dead.
  [loginButton_ setTarget:nil];
  [cancelButton_ setTarget:nil];
  [super dealloc];
}

- (IBAction)loginPressed:(id)sender {
  using base::SysNSStringToWide;
  [NSApp endSheet:[self window]];
  handler_->OnLoginPressed(
      base::SysNSStringToUTF16([nameField_ stringValue]),
      base::SysNSStringToUTF16([passwordField_ stringValue]));
}

- (IBAction)cancelPressed:(id)sender {
  [NSApp endSheet:[self window]];
  handler_->OnCancelPressed();
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void *)contextInfo {
  [sheet orderOut:self];
  // Also called when user navigates to another page while the sheet is open.
}

- (void)autofillLogin:(NSString*)login password:(NSString*)password {
  if ([[nameField_ stringValue] length] == 0) {
    [nameField_ setStringValue:login];
    [passwordField_ setStringValue:password];
    [nameField_ selectText:self];
  }
}

- (void)setExplanation:(NSString*)explanation {
  // Put in the text.
  [explanationField_ setStringValue:explanation];

  // Resize the TextField.
  CGFloat explanationShift =
      [GTMUILocalizerAndLayoutTweaker
       sizeToFitFixedWidthTextField:explanationField_];

  // Resize the window (no shifting needed due to window layout).
  NSSize windowDelta = NSMakeSize(0, explanationShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:windowDelta];
}

@end
