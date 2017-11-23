// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class SignInObserver;
}  // namespace

@interface BookmarkPromoController () {
  bool _isIncognito;
  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<SignInObserver> _signinObserver;
  bool _promoDisplayedRecorded;
}

// Presenter which can show signin UI.
@property(nonatomic, readonly, weak) id<SigninPresenter> presenter;

// Records that the promo was displayed. Can be called several times per
// instance but will effectively record the user action only once per instance.
- (void)recordPromoDisplayed;

// SignInObserver Callbacks

// Called when a user signs into Google services such as sync.
- (void)googleSigninSucceededWithAccountId:(const std::string&)account_id
                                  username:(const std::string&)username;

// Called when the currently signed-in user for a user has been signed out.
- (void)googleSignedOutWithAcountId:(const std::string&)account_id
                           username:(const std::string&)username;

@end

namespace {
class SignInObserver : public SigninManagerBase::Observer {
 public:
  SignInObserver(BookmarkPromoController* controller)
      : controller_(controller) {}

  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override {
    [controller_ googleSigninSucceededWithAccountId:account_id
                                           username:username];
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    [controller_ googleSignedOutWithAcountId:account_id username:username];
  }

 private:
  __weak BookmarkPromoController* controller_;
};
}  // namespace

@implementation BookmarkPromoController

@synthesize delegate = _delegate;
@synthesize promoState = _promoState;
@synthesize presenter = _presenter;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate
                           presenter:(id<SigninPresenter>)presenter {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _presenter = presenter;
    // Incognito browserState can go away before this class is released, this
    // code avoids keeping a pointer to it.
    _isIncognito = browserState->IsOffTheRecord();
    if (!_isIncognito) {
      _browserState = browserState;
      _signinObserver.reset(new SignInObserver(self));
      SigninManager* signinManager =
          ios::SigninManagerFactory::GetForBrowserState(_browserState);
      signinManager->AddObserver(_signinObserver.get());
    }
    [self updatePromoState];
  }
  return self;
}

- (void)dealloc {
  if (!_isIncognito) {
    DCHECK(_browserState);
    SigninManager* signinManager =
        ios::SigninManagerFactory::GetForBrowserState(_browserState);
    signinManager->RemoveObserver(_signinObserver.get());
  }
}

- (void)showSignInFromViewController:(UIViewController*)baseViewController {
  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_FromBookmarkManager"));
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_BOOKMARK_MANAGER];
  [self.presenter showSignin:command];
}

- (void)hidePromoCell {
  DCHECK(!_isIncognito);
  DCHECK(_browserState);
  self.promoState = NO;
}

- (void)setPromoState:(BOOL)promoState {
  if (_promoState != promoState) {
    _promoState = promoState;
    [self.delegate promoStateChanged:promoState];
  }
}

- (void)updatePromoState {
  self.promoState = NO;
  if (_isIncognito)
    return;

  DCHECK(_browserState);
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                         browserState:_browserState]) {
    SigninManager* signinManager =
        ios::SigninManagerFactory::GetForBrowserState(_browserState);
    self.promoState = !signinManager->IsAuthenticated();
    if (self.promoState)
      [self recordPromoDisplayed];
  }
}

#pragma mark - Private

- (void)recordPromoDisplayed {
  if (_promoDisplayedRecorded)
    return;
  _promoDisplayedRecorded = YES;
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromBookmarkManager"));
}

#pragma mark - SignInObserver

// Called when a user signs into Google services such as sync.
- (void)googleSigninSucceededWithAccountId:(const std::string&)account_id
                                  username:(const std::string&)username {
  self.promoState = NO;
}

// Called when the currently signed-in user for a user has been signed out.
- (void)googleSignedOutWithAcountId:(const std::string&)account_id
                           username:(const std::string&)username {
  [self updatePromoState];
}

@end
