// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/autofill/password_generation_manager.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebString;

namespace autofill {

class TestPasswordGenerationManager : public PasswordGenerationManager {
 public:
  explicit TestPasswordGenerationManager(content::RenderView* view)
      : PasswordGenerationManager(view) {}
  virtual ~TestPasswordGenerationManager() {}

 protected:
  virtual bool ShouldAnalyzeFrame(const WebKit::WebFrame& frame) const
      OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationManager);
};

class PasswordGenerationManagerTest : public ChromeRenderViewTest {
 public:
  PasswordGenerationManagerTest() {}

  virtual void SetUp() {
    // We don't actually create a PasswordGenerationManager during
    // ChromeRenderViewTest::SetUp because it's behind a flag. Since we want
    // to use a test manager anyway, we just create out own.
    ChromeRenderViewTest::SetUp();
    generation_manager_.reset(new TestPasswordGenerationManager(view_));
  }

 private:
  scoped_ptr<PasswordGenerationManager> generation_manager_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManagerTest);
};


TEST_F(PasswordGenerationManagerTest, DetectionTest) {
  const char kSigninFormHTML[] =
      "<FORM name = 'blah' action = 'www.random.com/'> "
      "  <INPUT type = 'text' id = 'username'/> "
      "  <INPUT type = 'password' id = 'password'/> "
      "  <INPUT type = 'submit' value = 'LOGIN' />"
      "</FORM>";
  LoadHTML(kSigninFormHTML);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement password_element = element.to<WebInputElement>();
  EXPECT_FALSE(password_element.isAutofilled());

  const char kAccountCreationFormHTML[] =
      "<FORM name = 'blah' action = 'www.random.com/'> "
      "  <INPUT type = 'text' id = 'username'/> "
      "  <INPUT type = 'password' id = 'first_password'/> "
      "  <INPUT type = 'password' id = 'second_password'/> "
      "  <INPUT type = 'submit' value = 'LOGIN' />"
      "</FORM>";
  LoadHTML(kAccountCreationFormHTML);

  // We don't autofill at first.
  document = GetMainFrame()->document();
  element = document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();
  EXPECT_FALSE(first_password_element.isAutofilled());
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement second_password_element = element.to<WebInputElement>();
  EXPECT_FALSE(second_password_element.isAutofilled());

  // After first element is focused, then we autofill.
  SetFocused(first_password_element.to<WebNode>());
  EXPECT_TRUE(first_password_element.isAutofilled());
  EXPECT_TRUE(second_password_element.isAutofilled());
}

}  // namespace autofill
