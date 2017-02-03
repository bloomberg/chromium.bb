// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"

using ::testing::_;
using ::testing::Invoke;

namespace {

void FakeGetAccessToken(ChromeIdentity*,
                        const std::string&,
                        const std::string&,
                        const std::set<std::string>&,
                        const ios::AccessTokenCallback& callback) {
  base::mac::ScopedBlock<ios::AccessTokenCallback> safe_callback(
      [callback copy]);

  // |GetAccessToken| is normally an asynchronous operation (that requires some
  // network calls), this is replicated here by dispatching it.
  dispatch_async(dispatch_get_main_queue(), ^{
    // Token and expiration date. It should be larger than typical test
    // execution because tests usually setup mock to expect one token request
    // and then rely on access token being served from cache.
    NSTimeInterval expiration = 60.0;
    NSDate* expiresDate = [NSDate dateWithTimeIntervalSinceNow:expiration];
    NSString* token = [expiresDate description];

    safe_callback.get()(token, expiresDate, nil);
  });
}

UIImage* FakeGetCachedAvatarForIdentity(ChromeIdentity*) {
  ios::SigninResourcesProvider* provider =
      ios::GetChromeBrowserProvider()->GetSigninResourcesProvider();
  return provider ? provider->GetDefaultAvatar() : nil;
}

void FakeGetAvatarForIdentity(ChromeIdentity* identity,
                              ios::GetAvatarCallback callback) {
  // |GetAvatarForIdentity| is normally an asynchronous operation, this is
  // replicated here by dispatching it.
  dispatch_async(dispatch_get_main_queue(), ^{
    callback(FakeGetCachedAvatarForIdentity(identity));
  });
}

void FakeGetHostedDomainForIdentity(ChromeIdentity* identity,
                                    ios::GetHostedDomainCallback callback) {
  NSString* domain = base::SysUTF8ToNSString(gaia::ExtractDomainName(
      gaia::CanonicalizeEmail(base::SysNSStringToUTF8(identity.userEmail))));

  // |GetHostedDomainForIdentity| is normally an asynchronous operation , this
  // is replicated here by dispatching it.
  dispatch_async(dispatch_get_main_queue(), ^{
    callback(domain, nil);
  });
}
}

@interface FakeAccountDetailsViewController : UIViewController {
  ChromeIdentity* _identity;  // Weak.
  base::scoped_nsobject<UIButton> _removeAccountButton;
}
@end

@implementation FakeAccountDetailsViewController

- (instancetype)initWithIdentity:(ChromeIdentity*)identity {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _identity = identity;
  }
  return self;
}

- (void)dealloc {
  [_removeAccountButton removeTarget:self
                              action:@selector(didTapRemoveAccount:)
                    forControlEvents:UIControlEventTouchUpInside];
  [super dealloc];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnoxious color, this is a test screen.
  self.view.backgroundColor = [UIColor orangeColor];

  _removeAccountButton.reset(
      [[UIButton buttonWithType:UIButtonTypeCustom] retain]);
  [_removeAccountButton setTitle:@"Remove account"
                        forState:UIControlStateNormal];
  [_removeAccountButton addTarget:self
                           action:@selector(didTapRemoveAccount:)
                 forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_removeAccountButton];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  CGRect bounds = self.view.bounds;
  [_removeAccountButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds))];
  [_removeAccountButton sizeToFit];
}

- (void)didTapRemoveAccount:(id)sender {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(_identity, ^(NSError*) {
        [self dismissViewControllerAnimated:YES completion:nil];
      });
}

@end

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

base::scoped_nsobject<UINavigationController>
FakeChromeIdentityService::NewAccountDetails(
    ChromeIdentity* identity,
    id<ChromeIdentityBrowserOpener> browser_opener) {
  base::scoped_nsobject<UIViewController> accountDetailsViewController(
      [[FakeAccountDetailsViewController alloc] initWithIdentity:identity]);
  base::scoped_nsobject<UINavigationController> navigationController(
      [[UINavigationController alloc]
          initWithRootViewController:accountDetailsViewController]);
  return navigationController;
}

base::scoped_nsobject<ChromeIdentityInteractionManager>
FakeChromeIdentityService::NewChromeIdentityInteractionManager(
    ios::ChromeBrowserState* browser_state,
    id<ChromeIdentityInteractionManagerDelegate> delegate) const {
  base::scoped_nsobject<ChromeIdentityInteractionManager> manager(
      [[FakeChromeIdentityInteractionManager alloc] init]);
  manager.get().delegate = delegate;
  return manager;
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
    // require some network calls), this is replicated here by dispatching
    // it.
    dispatch_async(dispatch_get_main_queue(), ^{
      callback(nil);
    });
  }
}

void FakeChromeIdentityService::GetAccessToken(
    ChromeIdentity* identity,
    const std::string& client_id,
    const std::string& client_secret,
    const std::set<std::string>& scopes,
    const ios::AccessTokenCallback& callback) {
  FakeGetAccessToken(identity, client_id, client_secret, scopes, callback);
}

UIImage* FakeChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  return FakeGetCachedAvatarForIdentity(identity);
}

void FakeChromeIdentityService::GetAvatarForIdentity(
    ChromeIdentity* identity,
    GetAvatarCallback callback) {
  FakeGetAvatarForIdentity(identity, callback);
}

void FakeChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {
  FakeGetHostedDomainForIdentity(identity, callback);
}

void FakeChromeIdentityService::SetUpForIntegrationTests() {}

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
