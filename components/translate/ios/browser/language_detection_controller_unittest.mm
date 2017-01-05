// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/language_detection_controller.h"

#include "base/mac/bind_objc_block.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/common/translate_pref_names.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

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

    base::scoped_nsobject<MockJsLanguageDetectionManager> js_manager(
        [[MockJsLanguageDetectionManager alloc] init]);
    controller_.reset(new LanguageDetectionController(
        &web_state_, js_manager.get(), &prefs_));
  }

  LanguageDetectionController* controller() { return controller_.get(); }

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

  __block bool block_was_called = false;
  auto subscription =
      controller()->RegisterLanguageDetectionCallback(base::BindBlock(
          ^(const LanguageDetectionController::DetectionDetails& details) {
            block_was_called = true;
            EXPECT_EQ(kRootLanguage, details.html_root_language);
            EXPECT_EQ(kContentLanguage, details.content_language);
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

}  // namespace translate
