// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gmock/include/gmock/gmock.h"

@class NSMutableArray;

namespace ios {

// A fake ChromeIdentityService used for testing.
class FakeChromeIdentityService : public ChromeIdentityService {
 public:
  FakeChromeIdentityService();
  virtual ~FakeChromeIdentityService();

  // Convenience method that returns the instance of
  // |FakeChromeIdentityService| from the ChromeBrowserProvider.
  static FakeChromeIdentityService* GetInstanceFromChromeProvider();

  // ChromeIdentityService implementation.
  base::scoped_nsobject<UINavigationController> NewAccountDetails(
      ChromeIdentity* identity,
      id<ChromeIdentityBrowserOpener> browser_opener) override;
  base::scoped_nsobject<ChromeIdentityInteractionManager>
  NewChromeIdentityInteractionManager(
      ios::ChromeBrowserState* browser_state,
      id<ChromeIdentityInteractionManagerDelegate> delegate) const override;

  bool IsValidIdentity(ChromeIdentity* identity) const override;
  ChromeIdentity* GetIdentityWithGaiaID(
      const std::string& gaia_id) const override;
  bool HasIdentities() const override;
  NSArray* GetAllIdentities() const override;
  NSArray* GetAllIdentitiesSortedForDisplay() const override;
  void ForgetIdentity(ChromeIdentity* identity,
                      ForgetIdentityCallback callback) override;

  virtual void GetAccessToken(
      ChromeIdentity* identity,
      const std::string& client_id,
      const std::string& client_secret,
      const std::set<std::string>& scopes,
      const ios::AccessTokenCallback& callback) override;

  virtual void GetAvatarForIdentity(ChromeIdentity* identity,
                                    GetAvatarCallback callback) override;

  virtual UIImage* GetCachedAvatarForIdentity(
      ChromeIdentity* identity) override;

  virtual void GetHostedDomainForIdentity(
      ChromeIdentity* identity,
      GetHostedDomainCallback callback) override;

  MOCK_METHOD1(GetMDMDeviceStatus,
               ios::MDMDeviceStatus(NSDictionary* user_info));

  MOCK_METHOD3(HandleMDMNotification,
               bool(ChromeIdentity* identity,
                    NSDictionary* user_info,
                    ios::MDMStatusCallback callback));

  // Sets up the mock methods for integration tests.
  void SetUpForIntegrationTests();

  // Adds the identities given their name.
  void AddIdentities(NSArray* identitiesNames);

  // Adds |identity| to the available identities.
  void AddIdentity(ChromeIdentity* identity);

 private:
  base::scoped_nsobject<NSMutableArray> identities_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_
