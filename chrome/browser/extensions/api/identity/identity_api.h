// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

class GetAuthTokenFunctionTest;
class MockGetAuthTokenFunction;
class GoogleServiceAuthError;
class Profile;

namespace extensions {

namespace identity_constants {
extern const char kInvalidClientId[];
extern const char kInvalidScopes[];
extern const char kAuthFailure[];
extern const char kNoGrant[];
extern const char kUserRejected[];
extern const char kUserNotSignedIn[];
extern const char kInteractionRequired[];
extern const char kInvalidRedirect[];
}  // namespace identity_constants

class IdentityGetAuthTokenFunction : public AsyncExtensionFunction,
                                     public OAuth2MintTokenFlow::Delegate,
                                     public ExtensionInstallPrompt::Delegate,
                                     public IdentitySigninFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN)

  IdentityGetAuthTokenFunction();

 protected:
  virtual ~IdentityGetAuthTokenFunction();

 private:
  friend class GetAuthTokenFunctionTest;
  friend class MockGetAuthTokenFunction;

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // OAuth2MintTokenFlow::Delegate implementation:
  virtual void OnMintTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnMintTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE;

  // IdentitySigninFlow::Delegate implementation:
  virtual void SigninSuccess(const std::string& token) OVERRIDE;
  virtual void SigninFailed() OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Starts a MintTokenFlow with the given mode.
  void StartFlow(OAuth2MintTokenFlow::Mode mode);

  virtual void ShowLoginPopup();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);
  // Caller owns the returned instance.
  virtual OAuth2MintTokenFlow* CreateMintTokenFlow(
      OAuth2MintTokenFlow::Mode mode);

  // Checks if there is a master login token to mint tokens for the extension.
  virtual bool HasLoginToken() const;

  bool should_prompt_for_scopes_;
  scoped_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  std::string refresh_token_;
  bool should_prompt_for_signin_;

  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_ptr<IdentitySigninFlow> signin_flow_;
};

class IdentityLaunchWebAuthFlowFunction : public AsyncExtensionFunction,
                                          public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW)

  IdentityLaunchWebAuthFlowFunction();

  // URL checking helpers. Public for testing.
  // Checks to see if the current URL ends the flow.
  bool IsFinalRedirectURL(const GURL& url) const;

  // Unit tests may override extension_id.
  void InitFinalRedirectURLPrefixesForTest(const std::string& extension_id);

 private:
  virtual ~IdentityLaunchWebAuthFlowFunction();
  virtual bool RunImpl() OVERRIDE;

  // WebAuthFlow::Delegate implementation.
  virtual void OnAuthFlowFailure(WebAuthFlow::Failure failure) OVERRIDE;
  virtual void OnAuthFlowURLChange(const GURL& redirect_url) OVERRIDE;

  // Helper to initialize final URLs vector.
  void InitFinalRedirectURLPrefixes(const std::string& extension_id);

  scoped_ptr<WebAuthFlow> auth_flow_;
  std::vector<GURL> final_prefixes_;
};

class IdentityAPI : public ProfileKeyedAPI,
                    public SigninGlobalError::AuthStatusProvider,
                    public content::NotificationObserver {
 public:
  explicit IdentityAPI(Profile* profile);
  virtual ~IdentityAPI();
  void Initialize();

  void ReportAuthError(const GoogleServiceAuthError& error);

  // ProfileKeyedAPI implementation.
  virtual void Shutdown() OVERRIDE;
  static ProfileKeyedAPIFactory<IdentityAPI>* GetFactoryInstance();

  // AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<IdentityAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "IdentityAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* profile_;
  SigninManager* signin_manager_;
  GoogleServiceAuthError error_;
  // Used to listen to notifications from the TokenService.
  content::NotificationRegistrar registrar_;
};

template <>
void ProfileKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
