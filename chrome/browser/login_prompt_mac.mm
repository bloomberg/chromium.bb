// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"
#import "chrome/browser/login_prompt_mac.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/cocoa/constrained_window_mac.h"
#include "chrome/browser/login_model.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerMac

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerMac : public LoginHandler,
                        public base::RefCountedThreadSafe<LoginHandlerMac>,
                        public ConstrainedWindowMacDelegateCustomSheet,
                        public LoginModelObserver {
 public:
  LoginHandlerMac(URLRequest* request)
      : handled_auth_(false),
        dialog_(NULL),
        request_(request),
        password_manager_(NULL),
        sheet_controller_(nil),
        login_model_(NULL) {
    // This constructor is called on the I/O thread, so we cannot load the nib
    // here. BuildViewForPasswordManager() will be invoked on the UI thread
    // later, so wait with loading the nib until then.
    DCHECK(request_) << "LoginHandlerMac constructed with NULL request";

    AddRef();  // matched by ReleaseLater.
    if (!ResourceDispatcherHost::RenderViewForRequest(request_,
                                                      &render_process_host_id_,
                                                      &tab_contents_id_)) {
      NOTREACHED();
    }
  }

  virtual ~LoginHandlerMac() {
    if (login_model_)
      login_model_->SetObserver(NULL);
  }

  void SetModel(LoginModel* model) {
    if (login_model_)
      login_model_->SetObserver(NULL);
    login_model_ = model;
    if (login_model_)
      login_model_->SetObserver(this);
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) {
    [sheet_controller_ autofillLogin:base::SysWideToNSString(username)
                            password:base::SysWideToNSString(password)];
  }

  // LoginHandler:
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           std::wstring explanation) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    // Load nib here instead of in constructor.
    sheet_controller_ = [[[LoginHandlerSheet alloc]
        initWithLoginHandler:this] autorelease];
    init([sheet_controller_ window], sheet_controller_,
          @selector(sheetDidEnd:returnCode:contextInfo:));

    SetModel(manager);

    [sheet_controller_ setExplanation:base::SysWideToNSString(explanation)];

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    dialog_ = GetTabContentsForLogin()->CreateConstrainedDialog(this);

    SendNotifications();
  }

  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) {
    password_form_ = form;
  }

  virtual void SetPasswordManager(PasswordManager* password_manager) {
    password_manager_ = password_manager;
  }

  virtual TabContents* GetTabContentsForLogin() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    return tab_util::GetTabContentsByID(render_process_host_id_,
                                        tab_contents_id_);
  }

  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password) {
    if (WasAuthHandled(true))
      return;

    // Tell the password manager the credentials were submitted / accepted.
    if (password_manager_) {
      password_form_.username_value = WideToUTF16Hack(username);
      password_form_.password_value = WideToUTF16Hack(password);
      password_manager_->ProvisionallySavePassword(password_form_);
    }

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
            NewRunnableMethod(this, &LoginHandlerMac::CloseContentsDeferred));
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerMac::SendNotifications));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &LoginHandlerMac::SetAuthDeferred, username, password));
  }

  virtual void CancelAuth() {
    if (WasAuthHandled(true))
      return;

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerMac::CloseContentsDeferred));
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerMac::SendNotifications));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerMac::CancelAuthDeferred));
  }

  virtual void OnRequestCancelled() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO)) <<
        "Why is OnRequestCancelled called from the UI thread?";

    // Reference is no longer valid.
    request_ = NULL;

    // Give up on auth if the request was cancelled.
    CancelAuth();
  }

  // Overridden from ConstrainedWindowMacDelegate:
  virtual void DeleteDelegate() {
    if (!WasAuthHandled(true)) {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(this, &LoginHandlerMac::CancelAuthDeferred));
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this, &LoginHandlerMac::SendNotifications));
    }

    // Close sheet if it's still open, as required by
    // ConstrainedWindowMacDelegate.
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    if (is_sheet_open())
      [NSApp endSheet:sheet()];

    SetModel(NULL);

    // Delete this object once all InvokeLaters have been called.
    ChromeThread::ReleaseSoon(ChromeThread::IO, FROM_HERE, this);
  }

  void OnLoginPressed(const std::wstring& username,
                      const std::wstring& password) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    SetAuth(username, password);
  }

  void OnCancelPressed() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    CancelAuth();
  }

 private:
  friend class LoginPrompt;

  // Calls SetAuth from the IO loop.
  void SetAuthDeferred(const std::wstring& username,
                       const std::wstring& password) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    if (request_) {
      request_->SetAuth(username, password);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Calls CancelAuth from the IO loop.
  void CancelAuthDeferred() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    if (request_) {
      request_->CancelAuth();
      // Verify that CancelAuth does destroy the request via our delegate.
      DCHECK(request_ != NULL);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Closes the view_contents from the UI loop.
  void CloseContentsDeferred() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    // The hosting ConstrainedWindow may have been freed.
    if (dialog_)
      dialog_->CloseConstrainedWindow();
  }

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  // If |set_handled| is true, it will mark authentication as handled.
  bool WasAuthHandled(bool set_handled) {
    AutoLock lock(handled_auth_lock_);
    bool was_handled = handled_auth_;
    if (set_handled)
      handled_auth_ = true;
    return was_handled;
  }

  // Notify observers that authentication is needed or received.  The automation
  // proxy uses this for testing.
  void SendNotifications() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    NotificationService* service = NotificationService::current();
    TabContents* requesting_contents = GetTabContentsForLogin();
    if (!requesting_contents)
      return;

    NavigationController* controller = &requesting_contents->controller();

    if (!WasAuthHandled(false)) {
      LoginNotificationDetails details(this);
      service->Notify(NotificationType::AUTH_NEEDED,
                      Source<NavigationController>(controller),
                      Details<LoginNotificationDetails>(&details));
    } else {
      service->Notify(NotificationType::AUTH_SUPPLIED,
                      Source<NavigationController>(controller),
                      NotificationService::NoDetails());
    }
  }

  // True if we've handled auth (SetAuth or CancelAuth has been called).
  bool handled_auth_;
  Lock handled_auth_lock_;

  // The ConstrainedWindow that is hosting our LoginView.
  // This should only be accessed on the UI loop.
  ConstrainedWindow* dialog_;

  // The request that wants login data.
  // This should only be accessed on the IO loop.
  URLRequest* request_;

  // The PasswordForm sent to the PasswordManager. This is so we can refer to it
  // when later notifying the password manager if the credentials were accepted
  // or rejected.
  // This should only be accessed on the UI loop.
  PasswordForm password_form_;

  // Points to the password manager owned by the TabContents requesting auth.
  // Can be null if the TabContents is not a TabContents.
  // This should only be accessed on the UI loop.
  PasswordManager* password_manager_;

  // Cached from the URLRequest, in case it goes NULL on us.
  int render_process_host_id_;
  int tab_contents_id_;

  // The Cocoa controller of the GUI.
  LoginHandlerSheet* sheet_controller_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  LoginModel* login_model_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerMac);
};

// static
LoginHandler* LoginHandler::Create(URLRequest* request) {
  return new LoginHandlerMac(request);
}

// ----------------------------------------------------------------------------
// LoginHandlerSheet

@implementation LoginHandlerSheet

- (id)initWithLoginHandler:(LoginHandlerMac*)handler {
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:@"HttpAuthLoginSheet"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath
                                     owner:self])) {
    handler_ = handler;
  }
  return self;
}

- (IBAction)loginPressed:(id)sender {
  using base::SysNSStringToWide;
  [NSApp endSheet:[self window]];
  handler_->OnLoginPressed(SysNSStringToWide([nameField_ stringValue]),
                           SysNSStringToWide([passwordField_ stringValue]));
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
