// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

#include "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#include "base/timer/mock_timer.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/wait_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Fake consent auditor used for the tests.
class FakeConsentAuditor : public consent_auditor::ConsentAuditor {
 public:
  static std::unique_ptr<KeyedService> CreateInstance(
      web::BrowserState* context) {
    ios::ChromeBrowserState* ios_context =
        ios::ChromeBrowserState::FromBrowserState(context);
    syncer::UserEventService* const user_event_service =
        IOSUserEventServiceFactory::GetForBrowserState(ios_context);
    return std::make_unique<FakeConsentAuditor>(
        ios_context->GetPrefs(), user_event_service,
        version_info::GetVersionNumber(),
        GetApplicationContext()->GetApplicationLocale());
  }

  FakeConsentAuditor(PrefService* pref_service,
                     syncer::UserEventService* user_event_service,
                     const std::string& app_version,
                     const std::string& app_locale)
      : ConsentAuditor(pref_service,
                       user_event_service,
                       app_version,
                       app_locale) {}
  ~FakeConsentAuditor() override {}

  void RecordGaiaConsent(consent_auditor::Feature feature,
                         const std::vector<int>& description_grd_ids,
                         int confirmation_string_id,
                         consent_auditor::ConsentStatus status) override {
    feature_ = feature;
    recorded_ids_ = description_grd_ids;
    confirmation_string_id_ = confirmation_string_id;
    status_ = status;
  }

  consent_auditor::Feature feature() const { return feature_; }
  const std::vector<int>& recorded_ids() const { return recorded_ids_; }
  int confirmation_string_id() const { return confirmation_string_id_; }
  consent_auditor::ConsentStatus status() const { return status_; }

 private:
  consent_auditor::Feature feature_;
  std::vector<int> recorded_ids_;
  int confirmation_string_id_ = -1;
  consent_auditor::ConsentStatus status_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsentAuditor);
};

// These tests verify that Chrome correctly records user's consent to Chrome
// Sync, which is a GDPR requirement. None of those tests should be turned off.
// If one of those tests fails, one of the following methods should be updated
// with the added or removed strings:
//   - ExpectedConsentStringIds()
//   - WhiteListLocalizedStrings()
class ChromeSigninViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                               gaiaID:@"foo1ID"
                                                 name:@"Fake Foo 1"];
    // Setup services.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFake::CreateAuthenticationService);
    builder.AddTestingFactory(ConsentAuditorFactory::GetInstance(),
                              FakeConsentAuditor::CreateInstance);
    context_ = builder.Build();
    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentity(identity_);
    fake_consent_auditor_ = static_cast<FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForBrowserState(context_.get()));
    // Setup view controller.
    vc_ = [[ChromeSigninViewController alloc]
        initWithBrowserState:context_.get()
                 accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
                 promoAction:signin_metrics::PromoAction::
                                 PROMO_ACTION_WITH_DEFAULT
              signInIdentity:identity_
                  dispatcher:nil];
    __block base::MockTimer* mock_timer_ptr = nullptr;
    vc_.timerGenerator = ^std::unique_ptr<base::Timer>(bool retain_user_task,
                                                       bool is_repeating) {
      auto mock_timer =
          std::make_unique<base::MockTimer>(retain_user_task, is_repeating);
      mock_timer_ptr = mock_timer.get();
      return mock_timer;
    };
    UIScreen* screen = [UIScreen mainScreen];
    UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
    [window makeKeyAndVisible];
    [window addSubview:[vc_ view]];
    ASSERT_TRUE(mock_timer_ptr);
    mock_timer_ptr->Fire();
    window_ = window;
  }

  // Adds in |string_set|, all the strings displayed by |view| and its subviews,
  // recursively.
  static void AddStringsFromView(NSMutableSet<NSString*>* string_set,
                                 UIView* view) {
    for (UIView* subview in view.subviews)
      AddStringsFromView(string_set, subview);
    if ([view isKindOfClass:[UIButton class]]) {
      UIButton* button = static_cast<UIButton*>(view);
      if (button.currentTitle)
        [string_set addObject:button.currentTitle];
    } else if ([view isKindOfClass:[UILabel class]]) {
      UILabel* label = static_cast<UILabel*>(view);
      if (label.text)
        [string_set addObject:label.text];
    } else {
      NSString* view_name = NSStringFromClass([view class]);
      // Views that don't display strings.
      NSArray* other_views = @[
        @"AccountControlCell", @"CollectionViewFooterCell", @"UIButtonLabel",
        @"UICollectionView", @"UICollectionViewControllerWrapperView",
        @"UIImageView", @"UIView", @"MDCActivityIndicator", @"MDCButtonBar",
        @"MDCFlexibleHeaderView", @"MDCHeaderStackView", @"MDCInkView",
        @"MDCNavigationBar"
      ];
      // If this test fails, the unknown class should be added in other_views if
      // it doesn't display any strings, otherwise the strings diplay by this
      // class should be added in string_set.
      EXPECT_TRUE([other_views containsObject:view_name]);
    }
  }

  // Returns the set of strings displayed on the screen based on the views
  // displayed by the .
  NSSet<NSString*>* LocalizedStringOnScreen() const {
    NSMutableSet* string_set = [NSMutableSet set];
    AddStringsFromView(string_set, vc_.view);
    return string_set;
  }

  // Returns a localized string based on the string id.
  NSString* LocalizedStringFromID(int string_id) const {
    NSString* string = l10n_util::GetNSString(string_id);
    string = [string stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                               withString:@""];
    string = [string stringByReplacingOccurrencesOfString:@"END_LINK"
                                               withString:@""];
    return string;
  }

  // Returns all the strings on screen that should be part of the user consent
  // and part of the white list strings.
  NSSet<NSString*>* LocalizedExpectedStringsOnScreen() const {
    const std::vector<int>& string_ids = ExpectedConsentStringIds();
    NSMutableSet<NSString*>* set = [NSMutableSet set];
    for (auto it = string_ids.begin(); it != string_ids.end(); ++it) {
      [set addObject:LocalizedStringFromID(*it)];
    }
    [set unionSet:WhiteListLocalizedStrings()];
    return set;
  }

  // Returns the list of string id that should be given to RecordGaiaConsent()
  // then the consent is given. The list is ordered according to the position
  // on the screen.
  const std::vector<int> ExpectedConsentStringIds() const {
    return {
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_TITLE,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_DESCRIPTION,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_TITLE,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_DESCRIPTION,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS,
    };
  }

  // Returns the white list of strings that can be displayed on screen but
  // should not be part of ExpectedConsentStringIds().
  NSSet<NSString*>* WhiteListLocalizedStrings() const {
    return [NSSet setWithObjects:@"Hi, Fake Foo 1", @"foo1@gmail.com",
                                 @"OK, GOT IT", @"UNDO", nil];
  }

  // Waits until all expected strings are on the screen.
  void WaitAndExpectAllStringsOnScreen() {
    ConditionBlock condition = ^bool() {
      return [LocalizedStringOnScreen()
          isEqual:LocalizedExpectedStringsOnScreen()];
    };
    EXPECT_TRUE(testing::WaitUntilConditionOrTimeout(10, condition));
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> context_;
  FakeChromeIdentity* identity_;
  UIWindow* window_;
  ChromeSigninViewController* vc_;
  FakeConsentAuditor* fake_consent_auditor_;
};

// Tests that all strings on the screen are either part of the consent string
// list defined in FakeConsentAuditor::ExpectedConsentStringIds()), or are part
// of the white list strings defined in
// FakeConsentAuditor::WhiteListLocalizedStrings().
TEST_F(ChromeSigninViewControllerTest, TestAllStrings) {
  WaitAndExpectAllStringsOnScreen();
}

// Tests when the user taps on "OK GOT IT", that RecordGaiaConsent() is called
// with the expected list of string ids, and
// IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON for the confirmation grd
// id.
TEST_F(ChromeSigninViewControllerTest, TestConsentWithOKGOTIT) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  const std::vector<int>& recorded_ids = fake_consent_auditor_->recorded_ids();
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON,
            fake_consent_auditor_->confirmation_string_id());
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->status());
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->feature());
}

// Tests that RecordGaiaConsent() is not called when the user taps on UNDO.
TEST_F(ChromeSigninViewControllerTest, TestRefusingConsent) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  const std::vector<int>& recorded_ids = fake_consent_auditor_->recorded_ids();
  EXPECT_EQ(0ul, recorded_ids.size());
  EXPECT_EQ(-1, fake_consent_auditor_->confirmation_string_id());
}

// Tests that RecordGaiaConsent() is called with the expected list of string
// ids, and IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS for the
// confirmation grd id.
TEST_F(ChromeSigninViewControllerTest, TestConsentWithSettings) {
  WaitAndExpectAllStringsOnScreen();
  [vc_ signinConfirmationControllerDidTapSettingsLink:vc_.confirmationVC];
  const std::vector<int>& recorded_ids = fake_consent_auditor_->recorded_ids();
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS,
            fake_consent_auditor_->confirmation_string_id());
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->status());
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->feature());
}

}  // namespace
