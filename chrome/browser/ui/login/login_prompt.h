// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_PROMPT_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_PROMPT_H_

#include <string>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"

class WebContentsModalDialog;
class GURL;

namespace content {
class RenderViewHostDelegate;
class NotificationRegistrar;
}  // namespace content

namespace net {
class AuthChallengeInfo;
class HttpNetworkSession;
class URLRequest;
}  // namespace net

// This is the base implementation for the OS-specific classes that route
// authentication info to the net::URLRequest that needs it. These functions
// must be implemented in a thread safe manner.
class LoginHandler : public content::ResourceDispatcherHostLoginDelegate,
                     public LoginModelObserver,
                     public content::NotificationObserver {
 public:
  LoginHandler(net::AuthChallengeInfo* auth_info, net::URLRequest* request);

  // Builds the platform specific LoginHandler. Used from within
  // CreateLoginPrompt() which creates tasks.
  static LoginHandler* Create(net::AuthChallengeInfo* auth_info,
                              net::URLRequest* request);

  // ResourceDispatcherHostLoginDelegate implementation:
  virtual void OnRequestCancelled() OVERRIDE;

  // Initializes the underlying platform specific view.
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           const string16& explanation) = 0;

  // Sets information about the authentication type (|form|) and the
  // |password_manager| for this profile.
  void SetPasswordForm(const content::PasswordForm& form);
  void SetPasswordManager(PasswordManager* password_manager);

  // Returns the WebContents that needs authentication.
  content::WebContents* GetWebContentsForLogin() const;

  // Resend the request with authentication credentials.
  // This function can be called from either thread.
  void SetAuth(const string16& username, const string16& password);

  // Display the error page without asking for credentials again.
  // This function can be called from either thread.
  void CancelAuth();

  // Implements the content::NotificationObserver interface.
  // Listens for AUTH_SUPPLIED and AUTH_CANCELLED notifications from other
  // LoginHandlers so that this LoginHandler has the chance to dismiss itself
  // if it was waiting for the same authentication.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Who/where/what asked for the authentication.
  const net::AuthChallengeInfo* auth_info() const { return auth_info_.get(); }

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  bool WasAuthHandled() const;

 protected:
  virtual ~LoginHandler();

  void SetModel(LoginModel* model);

  void SetDialog(WebContentsModalDialog* dialog);

  // Notify observers that authentication is needed.
  void NotifyAuthNeeded();

  // Performs necessary cleanup before deletion.
  void ReleaseSoon();

 private:
  // Starts observing notifications from other LoginHandlers.
  void AddObservers();

  // Stops observing notifications from other LoginHandlers.
  void RemoveObservers();

  // Notify observers that authentication is supplied.
  void NotifyAuthSupplied(const string16& username,
                          const string16& password);

  // Notify observers that authentication is cancelled.
  void NotifyAuthCancelled();

  // Marks authentication as handled and returns the previous handled
  // state.
  bool TestAndSetAuthHandled();

  // Calls SetAuth from the IO loop.
  void SetAuthDeferred(const string16& username,
                       const string16& password);

  // Calls CancelAuth from the IO loop.
  void CancelAuthDeferred();

  // Closes the view_contents from the UI loop.
  void CloseContentsDeferred();

  // True if we've handled auth (SetAuth or CancelAuth has been called).
  bool handled_auth_;
  mutable base::Lock handled_auth_lock_;

  // The WebContentsModalDialog that is hosting our LoginView.
  // This should only be accessed on the UI loop.
  WebContentsModalDialog* dialog_;

  // Who/where/what asked for the authentication.
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  // The request that wants login data.
  // This should only be accessed on the IO loop.
  net::URLRequest* request_;

  // The HttpNetworkSession |request_| is associated with.
  const net::HttpNetworkSession* http_network_session_;

  // The PasswordForm sent to the PasswordManager. This is so we can refer to it
  // when later notifying the password manager if the credentials were accepted
  // or rejected.
  // This should only be accessed on the UI loop.
  content::PasswordForm password_form_;

  // Points to the password manager owned by the WebContents requesting auth.
  // This should only be accessed on the UI loop.
  PasswordManager* password_manager_;

  // Cached from the net::URLRequest, in case it goes NULL on us.
  int render_process_host_id_;
  int tab_contents_id_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  LoginModel* login_model_;

  // Observes other login handlers so this login handler can respond.
  // This is only accessed on the UI thread.
  scoped_ptr<content::NotificationRegistrar> registrar_;
};

// Details to provide the content::NotificationObserver.  Used by the automation
// proxy for testing.
class LoginNotificationDetails {
 public:
  explicit LoginNotificationDetails(LoginHandler* handler)
      : handler_(handler) {}
  LoginHandler* handler() const { return handler_; }

 private:
  LoginNotificationDetails() {}

  LoginHandler* handler_;  // Where to send the response.

  DISALLOW_COPY_AND_ASSIGN(LoginNotificationDetails);
};

// Details to provide the NotificationObserver.  Used by the automation proxy
// for testing and by other LoginHandlers to dismiss themselves when an
// identical auth is supplied.
class AuthSuppliedLoginNotificationDetails : public LoginNotificationDetails {
 public:
  AuthSuppliedLoginNotificationDetails(LoginHandler* handler,
                                       const string16& username,
                                       const string16& password)
      : LoginNotificationDetails(handler),
        username_(username),
        password_(password) {}
  const string16& username() const { return username_; }
  const string16& password() const { return password_; }

 private:
  // The username that was used for the authentication.
  const string16 username_;

  // The password that was used for the authentication.
  const string16 password_;

  DISALLOW_COPY_AND_ASSIGN(AuthSuppliedLoginNotificationDetails);
};

// Prompts the user for their username and password.  This is designed to
// be called on the background (I/O) thread, in response to
// net::URLRequest::Delegate::OnAuthRequired.  The prompt will be created
// on the main UI thread via a call to UI loop's InvokeLater, and will send the
// credentials back to the net::URLRequest on the calling thread.
// A LoginHandler object (which lives on the calling thread) is returned,
// which can be used to set or cancel authentication programmatically.  The
// caller must invoke OnRequestCancelled() on this LoginHandler before
// destroying the net::URLRequest.
LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                net::URLRequest* request);

// Helper to remove the ref from an net::URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an
// net::URLRequest.
void ResetLoginHandlerForRequest(net::URLRequest* request);

// Get the signon_realm under which the identity should be saved.
std::string GetSignonRealm(const GURL& url,
                           const net::AuthChallengeInfo& auth_info);

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_PROMPT_H_
