// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/signin/cwv_authentication_controller_internal.h"
#import "ios/web_view/public/cwv_authentication_controller_delegate.h"
#import "ios/web_view/public/cwv_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    WebViewProfileOAuth2TokenServiceIOSProviderImpl(
        CWVAuthenticationController* controller)
    : controller_(controller) {}

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    ~WebViewProfileOAuth2TokenServiceIOSProviderImpl() = default;

void WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {
  if (![controller_ delegate]) {
    return;
  }
  AccessTokenCallback scoped_callback = callback;

  NSString* ns_gaia_id = base::SysUTF8ToNSString(gaia_id);
  NSString* ns_client_id = base::SysUTF8ToNSString(client_id);
  NSString* ns_client_secret = base::SysUTF8ToNSString(client_secret);
  NSMutableArray* scopes_array = [[NSMutableArray alloc] init];
  for (const auto& scope : scopes) {
    [scopes_array addObject:base::SysUTF8ToNSString(scope)];
  }
  void (^token_callback)(NSString*, NSDate*, NSError*) =
      ^void(NSString* token, NSDate* expiration, NSError* error) {
        if (!scoped_callback.is_null()) {
          scoped_callback.Run(token, expiration, error);
        }
      };

  [[controller_ delegate] authenticationController:controller_.get()
                           getAccessTokenForGaiaID:ns_gaia_id
                                          clientID:ns_client_id
                                      clientSecret:ns_client_secret
                                            scopes:scopes_array
                                 completionHandler:token_callback];
}

std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAllAccounts() const {
  if (![controller_ delegate]) {
    return {};
  }
  NSArray<CWVIdentity*>* identities = [[controller_ delegate]
      allIdentitiesForAuthenticationController:controller_.get()];
  std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo> accounts;
  for (CWVIdentity* identity in identities) {
    ProfileOAuth2TokenServiceIOSProvider::AccountInfo account;
    account.email = base::SysNSStringToUTF8(identity.email);
    account.gaia = base::SysNSStringToUTF8(identity.gaiaID);
    accounts.push_back(account);
  }
  return accounts;
}

AuthenticationErrorCategory
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  return kAuthenticationErrorCategoryUnknownErrors;
}
