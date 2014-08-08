// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/login_prompt_cocoa.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/tab_contents/tab_util.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "components/password_manager/core/browser/login_model.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"

using autofill::PasswordForm;
using content::BrowserThread;
using content::WebContents;

// ----------------------------------------------------------------------------
// LoginHandlerMac

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerMac : public LoginHandler,
                        public ConstrainedWindowMacDelegate {
 public:
  LoginHandlerMac(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request) {
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(
      const base::string16& username,
      const base::string16& password) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    [sheet_controller_ autofillLogin:base::SysUTF16ToNSString(username)
                            password:base::SysUTF16ToNSString(password)];
  }
  virtual void OnLoginModelDestroying() OVERRIDE {}

  // LoginHandler:
  virtual void BuildViewForPasswordManager(
      password_manager::PasswordManager* manager,
      const base::string16& explanation) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    sheet_controller_.reset(
        [[LoginHandlerSheet alloc] initWithLoginHandler:this]);

    SetModel(manager);

    [sheet_controller_ setExplanation:base::SysUTF16ToNSString(explanation)];

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    WebContents* requesting_contents = GetWebContentsForLogin();
    DCHECK(requesting_contents);

    base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
        [[CustomConstrainedWindowSheet alloc]
            initWithCustomWindow:[sheet_controller_ window]]);
    constrained_window_.reset(new ConstrainedWindowMac(
        this, requesting_contents, sheet));

    NotifyAuthNeeded();
  }

  virtual void CloseDialog() OVERRIDE {
    // The hosting dialog may have been freed.
    if (constrained_window_)
      constrained_window_->CloseWebContentsModalDialog();
  }

  // Overridden from ConstrainedWindowMacDelegate:
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    SetModel(NULL);
    ReleaseSoon();

    constrained_window_.reset();
    sheet_controller_.reset();
  }

  void OnLoginPressed(const base::string16& username,
                      const base::string16& password) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    SetAuth(username, password);
  }

  void OnCancelPressed() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CancelAuth();
  }

 private:
  friend class LoginPrompt;

  virtual ~LoginHandlerMac() {
    // This class will be deleted on a non UI thread. Ensure that the UI members
    // have already been deleted.
    CHECK(!constrained_window_.get());
    CHECK(!sheet_controller_.get());
  }

  // The Cocoa controller of the GUI.
  base::scoped_nsobject<LoginHandlerSheet> sheet_controller_;

  scoped_ptr<ConstrainedWindowMac> constrained_window_;

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
      [base::mac::FrameworkBundle() pathForResource:@"HttpAuthLoginSheet"
                                             ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath
                                     owner:self])) {
    handler_ = handler;
    // Force the nib to load so that all outlets are initialized.
    [self window];
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
  handler_->OnLoginPressed(
      base::SysNSStringToUTF16([nameField_ stringValue]),
      base::SysNSStringToUTF16([passwordField_ stringValue]));
}

- (IBAction)cancelPressed:(id)sender {
  handler_->OnCancelPressed();
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

  // Resize the text field.
  CGFloat windowDelta = [GTMUILocalizerAndLayoutTweaker
       sizeToFitFixedWidthTextField:explanationField_];

  NSRect newFrame = [[self window] frame];
  newFrame.size.height += windowDelta;
  [[self window] setFrame:newFrame display:NO];
}

@end
