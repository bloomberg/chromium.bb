// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/language_detection_controller.h"

#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MockJsLanguageDetectionManager : JsLanguageDetectionManager
@end

@implementation MockJsLanguageDetectionManager
- (void)retrieveBufferedTextContent:
        (const language_detection::BufferedTextCallback&)callback {
  callback.Run(base::UTF8ToUTF16("Some content"));
}
@end

namespace translate {

namespace {

class LanguageDetectionControllerTest : public PlatformTest {
 protected:
  LanguageDetectionControllerTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kEnableTranslate, true);

    MockJsLanguageDetectionManager* js_manager =
        [[MockJsLanguageDetectionManager alloc] init];
    controller_ = base::MakeUnique<LanguageDetectionController>(
        &web_state_, js_manager, &prefs_);
  }

  LanguageDetectionController* controller() { return controller_.get(); }
  web::TestWebState& web_state() { return web_state_; }

 private:
  TestingPrefServiceSimple prefs_;
  web::TestWebState web_state_;
  std::unique_ptr<LanguageDetectionController> controller_;
};

}  // namespace

// Tests that OnTextCaptured() correctly handles messages from the JS side and
// informs the driver.
TEST_F(LanguageDetectionControllerTest, OnTextCaptured) {
  const std::string kRootLanguage("en");
  const std::string kContentLanguage("fr");
  const std::string kUndefined("und");

  __block bool block_was_called = false;
  auto subscription =
      controller()->RegisterLanguageDetectionCallback(base::BindBlockArc(
          ^(const LanguageDetectionController::DetectionDetails& details) {
            block_was_called = true;
            EXPECT_EQ(kRootLanguage, details.html_root_language);
            EXPECT_EQ(kContentLanguage, details.content_language);
            EXPECT_FALSE(details.is_cld_reliable);
            EXPECT_EQ(kUndefined, details.cld_language);
          }));

  base::DictionaryValue command;
  command.SetString("command", "languageDetection.textCaptured");
  command.SetBoolean("translationAllowed", true);
  command.SetInteger("captureTextTime", 10);
  command.SetString("htmlLang", kRootLanguage);
  command.SetString("httpContentLanguage", kContentLanguage);
  controller()->OnTextCaptured(command, GURL("http://google.com"), false);

  EXPECT_TRUE(block_was_called);
}

// Tests that Content-Language response header is used if httpContentLanguage
// message value is empty.
TEST_F(LanguageDetectionControllerTest, MissingHttpContentLanguage) {
  // Pass content-language header to LanguageDetectionController.
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(""));
  headers->AddHeader("Content-Language: fr, en-CA");
  web::FakeNavigationContext context;
  context.SetResponseHeaders(headers);
  web_state().OnNavigationFinished(&context);

  __block bool block_was_called = false;
  auto subscription =
      controller()->RegisterLanguageDetectionCallback(base::BindBlockArc(
          ^(const LanguageDetectionController::DetectionDetails& details) {
            block_was_called = true;
            EXPECT_EQ("fr", details.content_language);
          }));

  base::DictionaryValue command;
  command.SetString("command", "languageDetection.textCaptured");
  command.SetBoolean("translationAllowed", true);
  command.SetInteger("captureTextTime", 10);
  command.SetString("htmlLang", "");
  command.SetString("httpContentLanguage", "");
  controller()->OnTextCaptured(command, GURL("http://google.com"), false);

  EXPECT_TRUE(block_was_called);
}

}  // namespace translate
