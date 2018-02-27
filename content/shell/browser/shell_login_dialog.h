// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_
#define CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"

#if defined(OS_MACOSX)
#if __OBJC__
@class ShellLoginDialogHelper;
#else
class ShellLoginDialogHelper;
#endif  // __OBJC__
#endif  // defined(OS_MACOSX)

namespace net {
class AuthChallengeInfo;
class URLRequest;
}

namespace content {

// This class provides a dialog box to ask the user for credentials. Useful in
// ResourceDispatcherHostDelegate::CreateLoginDelegate.
class ShellLoginDialog : public ResourceDispatcherHostLoginDelegate {
 public:
  // Threading: IO thread.
  ShellLoginDialog(net::AuthChallengeInfo* auth_info, net::URLRequest* request);

  // ResourceDispatcherHostLoginDelegate implementation:
  // Threading: IO thread.
  void OnRequestCancelled() override;

  // Called by the platform specific code when the user responds. Public because
  // the aforementioned platform specific code may not have access to private
  // members. Not to be called from client code.
  // Threading: UI thread.
  void UserAcceptedAuth(const base::string16& username,
                        const base::string16& password);
  void UserCancelledAuth();

 protected:
  // Threading: any
  ~ShellLoginDialog() override;

 private:
  // All the methods that begin with Platform need to be implemented by the
  // platform specific LoginDialog implementation.
  // Creates the dialog.
  // Threading: UI thread.
  void PlatformCreateDialog(const base::string16& message);
  // Called from the destructor to let each platform do any necessary cleanup.
  // Threading: UI thread.
  void PlatformCleanUp();
  // Called from OnRequestCancelled if the request was cancelled.
  // Threading: UI thread.
  void PlatformRequestCancelled();

  // Sets up dialog creation.
  // Threading: UI thread.
  void PrepDialog(const base::string16& host, const base::string16& realm);

  // Sends the authentication to the requester.
  // Threading: IO thread.
  void SendAuthToRequester(bool success,
                           const base::string16& username,
                           const base::string16& password);

  // Who/where/what asked for the authentication.
  // Threading: IO thread.
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  // The request that wants login data.
  // Threading: IO thread.
  net::URLRequest* request_;

#if defined(OS_MACOSX)
  // Threading: UI thread.
  ShellLoginDialogHelper* helper_;  // owned
#endif
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_
