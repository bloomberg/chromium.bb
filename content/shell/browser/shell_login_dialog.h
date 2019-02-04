// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_
#define CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"

#if defined(OS_MACOSX)
#if __OBJC__
@class ShellLoginDialogHelper;
#else
class ShellLoginDialogHelper;
#endif  // __OBJC__
#endif  // defined(OS_MACOSX)

namespace net {
class AuthChallengeInfo;
}

namespace content {

// This class provides a dialog box to ask the user for credentials. Useful in
// ContentBrowserClient::CreateLoginDelegate.
class ShellLoginDialog : public LoginDelegate {
 public:
  static std::unique_ptr<ShellLoginDialog> Create(
      net::AuthChallengeInfo* auth_info,
      LoginAuthRequiredCallback auth_required_callback);

  explicit ShellLoginDialog(LoginAuthRequiredCallback auth_required_callback);
  ~ShellLoginDialog() override;

  // Called by the platform specific code when the user responds. Public because
  // the aforementioned platform specific code may not have access to private
  // members. Not to be called from client code.
  void UserAcceptedAuth(const base::string16& username,
                        const base::string16& password);
  void UserCancelledAuth();

 private:
  void Init(net::AuthChallengeInfo* auth_info);

  // All the methods that begin with Platform need to be implemented by the
  // platform specific LoginDialog implementation.
  // Creates the dialog.
  void PlatformCreateDialog(const base::string16& message);
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();
  // Called from OnRequestCancelled if the request was cancelled.
  void PlatformRequestCancelled();

  // Sets up dialog creation.
  void PrepDialog(const base::string16& host, const base::string16& realm);

  // Sends the authentication to the requester.
  void SendAuthToRequester(bool success,
                           const base::string16& username,
                           const base::string16& password);

  LoginAuthRequiredCallback auth_required_callback_;

#if defined(OS_MACOSX)
  // Threading: UI thread.
  ShellLoginDialogHelper* helper_;  // owned
#endif
  base::WeakPtrFactory<ShellLoginDialog> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_LOGIN_DIALOG_H_
