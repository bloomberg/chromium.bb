// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/fakes/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
NSString* const kTestFromLangCode = @"ja";
NSString* const kTestToLangCode = @"en";
}  // namespace

class CWVTranslationControllerTest : public PlatformTest {
 protected:
  CWVTranslationControllerTest() : browser_state_(/*off_the_record=*/false) {
    web_state_.SetBrowserState(&browser_state_);
    auto test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    web_state_.SetNavigationManager(std::move(test_navigation_manager));
    CRWTestJSInjectionReceiver* injection_receiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state_.SetJSInjectionReceiver(injection_receiver);
    WebViewTranslateClient::CreateForWebState(&web_state_);
    translate_client_ = WebViewTranslateClient::FromWebState(&web_state_);
    translation_controller_ = [[CWVTranslationController alloc]
        initWithTranslateClient:translate_client_];
  }

  // Checks if |lang_code| matches the OCMArg's CWVTranslationLanguage.
  id CheckLanguageCode(NSString* lang_code) WARN_UNUSED_RESULT {
    return [OCMArg checkWithBlock:^BOOL(CWVTranslationLanguage* lang) {
      return [lang.languageCode isEqualToString:lang_code];
    }];
  }

  web::TestWebThreadBundle web_thread_bundle_;
  WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  WebViewTranslateClient* translate_client_;
  CWVTranslationController* translation_controller_;
};

// Tests CWVTranslationController invokes can offer delegate method.
TEST_F(CWVTranslationControllerTest, CanOfferCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  [[delegate expect] translationController:translation_controller_
           canOfferTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)];
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/false);

  [delegate verify];
}

// Tests CWVTranslationController invokes did start delegate method.
TEST_F(CWVTranslationControllerTest, DidStartCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  [[delegate expect] translationController:translation_controller_
           didStartTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)
                             userInitiated:YES];
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/true);

  [delegate verify];
}

// Tests CWVTranslationController invokes did finish delegate method.
TEST_F(CWVTranslationControllerTest, DidFinishCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  id check_error_code = [OCMArg checkWithBlock:^BOOL(NSError* error) {
    return error.code == CWVTranslationErrorInitializationError;
  }];
  [[delegate expect] translationController:translation_controller_
          didFinishTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)
                                     error:check_error_code];
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      base::SysNSStringToUTF8(kTestFromLangCode),
      base::SysNSStringToUTF8(kTestToLangCode),
      translate::TranslateErrors::INITIALIZATION_ERROR,
      /*triggered_from_menu=*/false);

  [delegate verify];
}

}  // namespace ios_web_view
