// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kAutomaticSigninPromoViewDismissCount = 20;

bool IsSupportedAccessPoint(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      return true;
    default:
      return false;
  }
}

void RecordSigninImpressionWithAccountUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithAccount_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithAccount_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImpressionWithAccount_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithAccount_FromTabSwitcher"));
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordSigninImpressionWithNoAccountUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithNoAccount_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithNoAccount_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithNoAccount_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_ImpressionWithNoAccount_FromTabSwitcher"));
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordSigninDefaultUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_SigninWithDefault_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninWithDefault_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninWithDefault_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninWithDefault_FromTabSwitcher"));
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordSigninNotDefaultUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_SigninNotDefault_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNotDefault_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNotDefault_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNotDefault_FromTabSwitcher"));
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordSigninNewAccountUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(base::UserMetricsAction(
          "Signin_SigninNewAccount_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNewAccount_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNewAccount_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(
          base::UserMetricsAction("Signin_SigninNewAccount_FromTabSwitcher"));
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordImpressionsTilSigninButtonsHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordImpressionsTilDismissHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss",
          displayed_count);
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

void RecordImpressionsTilXButtonHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      UMA_HISTOGRAM_COUNTS_100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilXButton",
          displayed_count);
      break;
    default:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

const char* DisplayedCountPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      return prefs::kIosBookmarkSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return prefs::kIosSettingsSigninPromoDisplayedCount;
    default:
      return nullptr;
  }
}

const char* AlreadySeenSigninViewPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      return prefs::kIosBookmarkPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return prefs::kIosSettingsPromoAlreadySeen;
    default:
      return nullptr;
  }
}
}  // namespace

@interface SigninPromoViewMediator ()<ChromeIdentityServiceObserver,
                                      ChromeBrowserProviderObserver>
// Presenter which can show signin UI.
@property(nonatomic, readonly, weak) id<SigninPresenter> presenter;
@end

@implementation SigninPromoViewMediator {
  ios::ChromeBrowserState* _browserState;
  signin_metrics::AccessPoint _accessPoint;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
  UIImage* _identityAvatar;
  BOOL _isSigninPromoViewVisible;
}

@synthesize consumer = _consumer;
@synthesize defaultIdentity = _defaultIdentity;
@synthesize signinPromoViewState = _signinPromoViewState;
@synthesize presenter = _presenter;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Bookmarks
  registry->RegisterBooleanPref(prefs::kIosBookmarkPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
  // Settings
  registry->RegisterBooleanPref(prefs::kIosSettingsPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosSettingsSigninPromoDisplayedCount,
                                0);
}

+ (BOOL)shouldDisplaySigninPromoViewWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                       browserState:(ios::ChromeBrowserState*)
                                                        browserState {
  PrefService* prefs = browserState->GetPrefs();
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(accessPoint);
  if (displayedCountPreferenceKey &&
      prefs->GetInteger(displayedCountPreferenceKey) >=
          kAutomaticSigninPromoViewDismissCount) {
    return NO;
  }
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(accessPoint);
  if (alreadySeenSigninViewPreferenceKey &&
      prefs->GetBoolean(alreadySeenSigninViewPreferenceKey)) {
    return NO;
  }
  return YES;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                           presenter:(id<SigninPresenter>)presenter {
  self = [super init];
  if (self) {
    DCHECK(IsSupportedAccessPoint(accessPoint));
    _accessPoint = accessPoint;
    _browserState = browserState;
    _presenter = presenter;
    NSArray* identities = ios::GetChromeBrowserProvider()
                              ->GetChromeIdentityService()
                              ->GetAllIdentitiesSortedForDisplay();
    if (identities.count != 0) {
      [self selectIdentity:identities[0]];
    }
    if (_defaultIdentity) {
      RecordSigninImpressionWithAccountUserActionForAccessPoint(accessPoint);
    } else {
      RecordSigninImpressionWithNoAccountUserActionForAccessPoint(accessPoint);
    }
    _identityServiceObserver =
        base::MakeUnique<ChromeIdentityServiceObserverBridge>(self);
    _browserProviderObserver =
        base::MakeUnique<ChromeBrowserProviderObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  DCHECK_EQ(ios::SigninPromoViewState::Invalid, _signinPromoViewState);
}

- (BOOL)isInvalidOrClosed {
  return _signinPromoViewState == ios::SigninPromoViewState::Closed ||
         _signinPromoViewState == ios::SigninPromoViewState::Invalid;
}

- (SigninPromoViewConfigurator*)createConfigurator {
  if (_defaultIdentity) {
    return [[SigninPromoViewConfigurator alloc]
        initWithUserEmail:_defaultIdentity.userEmail
             userFullName:_defaultIdentity.userFullName
                userImage:_identityAvatar];
  }
  return [[SigninPromoViewConfigurator alloc] initWithUserEmail:nil
                                                   userFullName:nil
                                                      userImage:nil];
}

- (void)selectIdentity:(ChromeIdentity*)identity {
  _defaultIdentity = identity;
  if (!_defaultIdentity) {
    _identityAvatar = nil;
  } else {
    __weak SigninPromoViewMediator* weakSelf = self;
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(identity, ^(UIImage* identityAvatar) {
          if (weakSelf.defaultIdentity != identity) {
            return;
          }
          [weakSelf identityAvatarUpdated:identityAvatar];
        });
  }
}

- (void)identityAvatarUpdated:(UIImage*)identityAvatar {
  _identityAvatar = identityAvatar;
  [self sendConsumerNotificationWithIdentityChanged:NO];
}

- (void)sendConsumerNotificationWithIdentityChanged:(BOOL)identityChanged {
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  [_consumer configureSigninPromoWithConfigurator:configurator
                                  identityChanged:identityChanged];
}

- (void)sendImpressionsTillSigninButtonsHistogram {
  DCHECK(![self isInvalidOrClosed] ||
         _signinPromoViewState != ios::SigninPromoViewState::Unused);
  _signinPromoViewState = ios::SigninPromoViewState::SigninStarted;
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(_accessPoint);
  if (!displayedCountPreferenceKey)
    return;
  PrefService* prefs = _browserState->GetPrefs();
  int displayedCount = prefs->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilSigninButtonsHistogramForAccessPoint(_accessPoint,
                                                           displayedCount);
}

- (void)signinPromoViewVisible {
  DCHECK(![self isInvalidOrClosed]);
  if (_isSigninPromoViewVisible)
    return;
  _isSigninPromoViewVisible = YES;
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(_accessPoint);
  if (!displayedCountPreferenceKey)
    return;
  PrefService* prefs = _browserState->GetPrefs();
  int displayedCount = prefs->GetInteger(displayedCountPreferenceKey);
  ++displayedCount;
  prefs->SetInteger(displayedCountPreferenceKey, displayedCount);
}

- (void)signinPromoViewHidden {
  DCHECK(![self isInvalidOrClosed]);
  _isSigninPromoViewVisible = NO;
}

- (void)signinPromoViewClosed {
  DCHECK(_isSigninPromoViewVisible && ![self isInvalidOrClosed]);
  _signinPromoViewState = ios::SigninPromoViewState::Closed;
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(_accessPoint);
  DCHECK(alreadySeenSigninViewPreferenceKey);
  PrefService* prefs = _browserState->GetPrefs();
  prefs->SetBoolean(alreadySeenSigninViewPreferenceKey, true);
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(_accessPoint);
  if (!displayedCountPreferenceKey)
    return;
  int displayedCount = prefs->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilXButtonHistogramForAccessPoint(_accessPoint,
                                                     displayedCount);
}

- (void)signinPromoViewRemoved {
  DCHECK_NE(ios::SigninPromoViewState::Invalid, _signinPromoViewState);
  BOOL wasUnused = _signinPromoViewState == ios::SigninPromoViewState::Unused;
  _signinPromoViewState = ios::SigninPromoViewState::Invalid;
  // If the sign-in promo view has been used at least once, it should not be
  // counted as dismissed (even if the sign-in has been canceled).
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(_accessPoint);
  if (!displayedCountPreferenceKey || !wasUnused)
    return;
  // If the sign-in view is removed when the user is authenticated, then the
  // sign-in has been done by another view, and this mediator cannot be counted
  // as being dismissed.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  if (authService->IsAuthenticated())
    return;
  PrefService* prefs = _browserState->GetPrefs();
  int displayedCount = prefs->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilDismissHistogramForAccessPoint(_accessPoint,
                                                     displayedCount);
}

- (void)signinCallback {
  DCHECK_EQ(ios::SigninPromoViewState::SigninStarted, _signinPromoViewState);
  _signinPromoViewState = ios::SigninPromoViewState::UsedAtLeastOnce;
  if ([_consumer respondsToSelector:@selector(signinDidFinish)])
    [_consumer signinDidFinish];
}

- (void)showSigninWithIdentity:(ChromeIdentity*)identity
                   promoAction:(signin_metrics::PromoAction)promoAction {
  __weak SigninPromoViewMediator* weakSelf = self;
  ShowSigninCommandCompletionCallback completion = ^(BOOL succeeded) {
    [weakSelf signinCallback];
  };
  if ([self.consumer respondsToSelector:@selector
                     (signinPromoViewMediator:shouldOpenSigninWithIdentity
                                                :promoAction:completion:)]) {
    [self.consumer signinPromoViewMediator:self
              shouldOpenSigninWithIdentity:identity
                               promoAction:promoAction
                                completion:completion];
  } else {
    ShowSigninCommand* command = [[ShowSigninCommand alloc]
        initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
                 identity:identity
              accessPoint:_accessPoint
              promoAction:promoAction
                 callback:completion];
    [self.presenter showSignin:command];
  }
}

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  ChromeIdentity* newIdentity = nil;
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  if (identities.count != 0) {
    newIdentity = identities[0];
  }
  if (newIdentity != _defaultIdentity) {
    [self selectIdentity:newIdentity];
    [self sendConsumerNotificationWithIdentityChanged:YES];
  }
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  if (identity == _defaultIdentity) {
    [self sendConsumerNotificationWithIdentityChanged:NO];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - ChromeBrowserProviderObserver

- (void)chromeIdentityServiceDidChange:(ios::ChromeIdentityService*)identity {
  DCHECK(!_identityServiceObserver.get());
  _identityServiceObserver =
      base::MakeUnique<ChromeIdentityServiceObserverBridge>(self);
}

- (void)chromeBrowserProviderWillBeDestroyed {
  _browserProviderObserver.reset();
}

#pragma mark - SigninPromoViewDelegate

- (void)signinPromoViewDidTapSigninWithNewAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(!_defaultIdentity && ![self isInvalidOrClosed]);
  [self sendImpressionsTillSigninButtonsHistogram];
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  RecordSigninNewAccountUserActionForAccessPoint(_accessPoint);
  [self showSigninWithIdentity:nil
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_NEW_ACCOUNT];
}

- (void)signinPromoViewDidTapSigninWithDefaultAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(_defaultIdentity && ![self isInvalidOrClosed]);
  [self sendImpressionsTillSigninButtonsHistogram];
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  RecordSigninDefaultUserActionForAccessPoint(_accessPoint);
  [self showSigninWithIdentity:_defaultIdentity
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_WITH_DEFAULT];
}

- (void)signinPromoViewDidTapSigninWithOtherAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(_defaultIdentity && ![self isInvalidOrClosed]);
  [self sendImpressionsTillSigninButtonsHistogram];
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  RecordSigninNotDefaultUserActionForAccessPoint(_accessPoint);
  [self showSigninWithIdentity:nil
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_NOT_DEFAULT];
}

@end
