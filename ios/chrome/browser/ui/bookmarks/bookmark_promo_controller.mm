// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/ui_util.h"

namespace {
// Enum is used to record the actions performed by the user on the promo cell.
// |Stars.PromoActions|.
enum {
  // Recorded each time the promo cell is presented to the user.
  BOOKMARKS_PROMO_ACTION_DISPLAYED,
  // The user selected the NO THANKS button.
  BOOKMARKS_PROMO_ACTION_DISMISSED,
  // The user selected the SIGN-IN button.
  BOOKMARKS_PROMO_ACTION_COMPLETED,
  // NOTE: Add new promo actions in sources only immediately above this line.
  // Also, make sure the enum list for histogram |Stars.PromoActions| in
  // histograms.xml is updated with any change in here.
  BOOKMARKS_PROMO_ACTION_COUNT
};

// The histogram used to record user actions performed on the promo cell.
const char kBookmarksPromoActionsHistogram[] = "Stars.PromoActions";

class SignInObserver;
}  // namespace

@interface BookmarkPromoController () {
  bool _isIncognito;
  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<SignInObserver> _signinObserver;
  bool _promoDisplayedRecorded;
}

// Records that the promo was displayed. Can be called several times per
// instance but will effectively record the histogram only once per instance.
- (void)recordPromoDisplayed;

// SignInObserver Callbacks

// Called when a user signs into Google services such as sync.
- (void)googleSigninSucceededWithAccountId:(const std::string&)account_id
                                  username:(const std::string&)username
                                  password:(const std::string&)password;

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
                             const std::string& username,
                             const std::string& password) override {
    [controller_ googleSigninSucceededWithAccountId:account_id
                                           username:username
                                           password:password];
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    [controller_ googleSignedOutWithAcountId:account_id username:username];
  }

 private:
  // Weak
  BookmarkPromoController* controller_;
};
}  // namespace

@implementation BookmarkPromoController

@synthesize delegate = _delegate;
@synthesize promoState = _promoState;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(prefs::kIosBookmarkPromoAlreadySeen, false);
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
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
  [super dealloc];
}

- (void)showSignIn {
  UMA_HISTOGRAM_ENUMERATION(kBookmarksPromoActionsHistogram,
                            BOOKMARKS_PROMO_ACTION_COMPLETED,
                            BOOKMARKS_PROMO_ACTION_COUNT);
  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_FromBookmarkManager"));
  base::scoped_nsobject<ShowSigninCommand> command([[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
      signInAccessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_BOOKMARK_MANAGER]);
  [self chromeExecuteCommand:command];
}

- (void)hidePromoCell {
  DCHECK(!_isIncognito);
  DCHECK(_browserState);

  UMA_HISTOGRAM_ENUMERATION(kBookmarksPromoActionsHistogram,
                            BOOKMARKS_PROMO_ACTION_DISMISSED,
                            BOOKMARKS_PROMO_ACTION_COUNT);
  PrefService* prefs = _browserState->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, true);
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
  PrefService* prefs = _browserState->GetPrefs();
  if (!prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen)) {
    SigninManager* signinManager =
        ios::SigninManagerFactory::GetForBrowserState(_browserState);
    self.promoState = !signinManager->IsAuthenticated();
    if (self.promoState)
      [self recordPromoDisplayed];
  }
}

#pragma mark - Private

- (void)chromeExecuteCommand:(id)sender {
  id delegate = [[UIApplication sharedApplication] delegate];
  if ([delegate respondsToSelector:@selector(chromeExecuteCommand:)])
    [delegate chromeExecuteCommand:sender];
}

- (void)recordPromoDisplayed {
  if (_promoDisplayedRecorded)
    return;
  _promoDisplayedRecorded = YES;
  UMA_HISTOGRAM_ENUMERATION(kBookmarksPromoActionsHistogram,
                            BOOKMARKS_PROMO_ACTION_DISPLAYED,
                            BOOKMARKS_PROMO_ACTION_COUNT);
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromBookmarkManager"));
}

#pragma mark - SignInObserver

// Called when a user signs into Google services such as sync.
- (void)googleSigninSucceededWithAccountId:(const std::string&)account_id
                                  username:(const std::string&)username
                                  password:(const std::string&)password {
  self.promoState = NO;
}

// Called when the currently signed-in user for a user has been signed out.
- (void)googleSignedOutWithAcountId:(const std::string&)account_id
                           username:(const std::string&)username {
  [self updatePromoState];
}

@end
