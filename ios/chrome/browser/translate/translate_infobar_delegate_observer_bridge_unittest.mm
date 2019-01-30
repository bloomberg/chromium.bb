// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"

#include <memory>

#include "components/language/core/browser/language_model.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateDriver;
using translate::testing::MockTranslateRanker;

class MockLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class MockTranslateInfoBarDelegate
    : public translate::TranslateInfoBarDelegate {
 public:
  MockTranslateInfoBarDelegate(
      const base::WeakPtr<translate::TranslateManager>& translate_manager,
      bool is_off_the_record,
      translate::TranslateStep step,
      const std::string& original_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool triggered_from_menu)
      : translate::TranslateInfoBarDelegate(translate_manager,
                                            is_off_the_record,
                                            step,
                                            original_language,
                                            target_language,
                                            error_type,
                                            triggered_from_menu) {}
  ~MockTranslateInfoBarDelegate() override {}

  MOCK_METHOD1(SetObserver, void(Observer* observer));
};

class MockTranslateInfoBarDelegateFactory {
 public:
  MockTranslateInfoBarDelegateFactory() {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    translate::TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    client_ =
        std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
    ranker_ = std::make_unique<MockTranslateRanker>();
    language_model_ = std::make_unique<MockLanguageModel>();
    manager_ = std::make_unique<translate::TranslateManager>(
        client_.get(), ranker_.get(), language_model_.get());
    delegate_ = std::make_unique<MockTranslateInfoBarDelegate>(
        manager_->GetWeakPtr(), false,
        translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
        translate::TranslateErrors::Type::NONE, false);
  }

  ~MockTranslateInfoBarDelegateFactory() = default;

  MockTranslateInfoBarDelegate* GetMockTranslateInfoBarDelegate() {
    return delegate_.get();
  }

 private:
  MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<MockLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> manager_;
  std::unique_ptr<MockTranslateInfoBarDelegate> delegate_;
};

@interface TestTranslateInfoBarDelegateObserver
    : NSObject <TranslateInfobarDelegateObserving>

@property(nonatomic) BOOL onTranslateStepChangedCalled;

@property(nonatomic) translate::TranslateStep translateStep;

@property(nonatomic) translate::TranslateErrors::Type errorType;

@property(nonatomic) BOOL onDidDismissWithoutInteractionCalled;

@end

@implementation TestTranslateInfoBarDelegateObserver

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  self.onTranslateStepChangedCalled = YES;
  self.translateStep = step;
  self.errorType = errorType;
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  self.onDidDismissWithoutInteractionCalled = YES;
  return YES;
}

@end

class TranslateInfobarDelegateObserverBridgeTest : public PlatformTest {
 protected:
  TranslateInfobarDelegateObserverBridgeTest()
      : observer_([[TestTranslateInfoBarDelegateObserver alloc] init]) {
    // Make sure the observer bridge sets up itself as an observer.
    EXPECT_CALL(*GetDelegate(), SetObserver(_)).Times(1);

    observer_bridge_ = std::make_unique<TranslateInfobarDelegateObserverBridge>(
        delegate_factory_.GetMockTranslateInfoBarDelegate(), observer_);
  }

  ~TranslateInfobarDelegateObserverBridgeTest() override {
    // Make sure the observer bridge removes itself as an observer.
    EXPECT_CALL(*GetDelegate(), SetObserver(nullptr)).Times(1);
  }

  MockTranslateInfoBarDelegate* GetDelegate() {
    return delegate_factory_.GetMockTranslateInfoBarDelegate();
  }

  translate::TranslateInfoBarDelegate::Observer* GetObserverBridge() {
    return observer_bridge_.get();
  }

  TestTranslateInfoBarDelegateObserver* GetDelegateObserver() {
    return observer_;
  }

 private:
  MockTranslateInfoBarDelegateFactory delegate_factory_;
  TestTranslateInfoBarDelegateObserver* observer_;
  std::unique_ptr<TranslateInfobarDelegateObserverBridge> observer_bridge_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfobarDelegateObserverBridgeTest);
};

// Tests that |OnTranslateStepChanged| call is forwarded by the observer bridge.
TEST_F(TranslateInfobarDelegateObserverBridgeTest, OnTranslateStepChanged) {
  ASSERT_FALSE(GetDelegateObserver().onTranslateStepChangedCalled);
  GetObserverBridge()->OnTranslateStepChanged(
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR,
      translate::TranslateErrors::Type::BAD_ORIGIN);
  EXPECT_TRUE(GetDelegateObserver().onTranslateStepChangedCalled);
  EXPECT_EQ(GetDelegateObserver().translateStep,
            translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR);
  EXPECT_EQ(GetDelegateObserver().errorType,
            translate::TranslateErrors::Type::BAD_ORIGIN);
}

// Tests that |IsDeclinedByUser| call is forwarded by the observer bridge.
TEST_F(TranslateInfobarDelegateObserverBridgeTest,
       DidDismissWithoutInteraction) {
  ASSERT_FALSE(GetDelegateObserver().onDidDismissWithoutInteractionCalled);
  GetObserverBridge()->IsDeclinedByUser();
  EXPECT_TRUE(GetDelegateObserver().onDidDismissWithoutInteractionCalled);
}
