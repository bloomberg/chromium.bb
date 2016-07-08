// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"

namespace ios {

NSString* const kIdentityEmailFormat = @"%@@foo.com";
NSString* const kIdentityGaiaIDFormat = @"%@ID";

FakeChromeIdentityService::FakeChromeIdentityService()
    : identities_([[NSMutableArray alloc] init]) {}

FakeChromeIdentityService::~FakeChromeIdentityService() {}

// static
FakeChromeIdentityService*
FakeChromeIdentityService::GetInstanceFromChromeProvider() {
  return static_cast<ios::FakeChromeIdentityService*>(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

bool FakeChromeIdentityService::IsValidIdentity(
    ChromeIdentity* identity) const {
  return [identities_ indexOfObject:identity] != NSNotFound;
}

ChromeIdentity* FakeChromeIdentityService::GetIdentityWithGaiaID(
    const std::string& gaia_id) const {
  NSString* gaiaID = base::SysUTF8ToNSString(gaia_id);
  NSUInteger index =
      [identities_ indexOfObjectPassingTest:^BOOL(ChromeIdentity* obj,
                                                  NSUInteger, BOOL* stop) {
        return [[obj gaiaID] isEqualToString:gaiaID];
      }];
  if (index == NSNotFound) {
    return nil;
  }
  return [identities_ objectAtIndex:index];
}

bool FakeChromeIdentityService::HasIdentities() const {
  return [identities_ count] > 0;
}

NSArray* FakeChromeIdentityService::GetAllIdentities() const {
  return identities_;
}

NSArray* FakeChromeIdentityService::GetAllIdentitiesSortedForDisplay() const {
  return identities_;
}

void FakeChromeIdentityService::ForgetIdentity(
    ChromeIdentity* identity,
    ForgetIdentityCallback callback) {
  [identities_ removeObject:identity];
  FireIdentityListChanged();
  if (callback) {
    // Forgetting an identity is normally an asynchronous operation (that
    // require some network calls), this is replicated here by dispatching it.
    dispatch_async(dispatch_get_main_queue(), ^{
      callback(nil);
    });
  }
}

void FakeChromeIdentityService::AddIdentities(NSArray* identitiesNames) {
  for (NSString* name in identitiesNames) {
    NSString* email = [NSString stringWithFormat:kIdentityEmailFormat, name];
    NSString* gaiaID = [NSString stringWithFormat:kIdentityGaiaIDFormat, name];
    [identities_ addObject:[FakeChromeIdentity identityWithEmail:email
                                                          gaiaID:gaiaID
                                                            name:name]];
  }
}

void FakeChromeIdentityService::AddIdentity(ChromeIdentity* identity) {
  [identities_ addObject:identity];
  FireIdentityListChanged();
}

}  // namespace ios
