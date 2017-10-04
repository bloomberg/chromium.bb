// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_passwords_analyser.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFormElement.h"

namespace autofill {

namespace {

class MockPagePasswordsAnalyserLogger : public PagePasswordsAnalyserLogger {
 public:
  void Send(const std::string& message,
            ConsoleLevel level,
            const blink::WebNode& node) override {
    Send(message, level, std::vector<blink::WebNode>{node});
  }

  MOCK_METHOD3(Send,
               void(const std::string& message,
                    ConsoleLevel level,
                    const std::vector<blink::WebNode>& nodes));

  MOCK_METHOD0(Flush, void());
};

const char kPasswordFieldNotInForm[] =
    "<input type='password' autocomplete='new-password'>";

const char kPasswordFormWithoutUsernameField[] =
    "<form>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const char kElementsWithDuplicateIds[] =
    "<input id='duplicate'>"
    "<input id='duplicate'>";

const char kPasswordFormTooComplex[] =
    "<form>"
    "   <input type='text' autocomplete='username'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='text' autocomplete='username'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const char kInferredPasswordAutocompleteAttributes[] =
    // Login form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "</form>"
    // Registration form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "</form>"
    // Change password form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "</form>";

const char kInferredUsernameAutocompleteAttributes[] =
    // Login form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='current-password'>"
    "</form>"
    // Registration form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>"
    // Change password form with username.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const std::string AutocompleteSuggestionString(const std::string& suggestion) {
  return "Input elements should have autocomplete "
         "attributes (suggested: \"" +
         suggestion + "\"):";
}

}  // namespace

class PagePasswordsAnalyserTest : public ChromeRenderViewTest {
 protected:
  PagePasswordsAnalyserTest() {}

  void TearDown() override {
    page_passwords_analyser.Reset();
    ChromeRenderViewTest::TearDown();
  }

  void LoadTestCase(const char* html) {
    elements_.clear();
    LoadHTML(html);
    blink::WebLocalFrame* frame = GetMainFrame();
    blink::WebElementCollection collection = frame->GetDocument().All();
    for (blink::WebElement element = collection.FirstItem(); !element.IsNull();
         element = collection.NextItem()) {
      elements_.push_back(element);
    }
    // Remove the <html>, <head> and <body> elements.
    elements_.erase(elements_.begin(), elements_.begin() + 3);
  }

  void Expect(const std::string& message,
              const ConsoleLevel level,
              const std::vector<size_t>& element_indices) {
    std::vector<blink::WebNode> nodes;
    for (size_t index : element_indices)
      nodes.push_back(elements_[index]);
    EXPECT_CALL(mock_logger, Send(message, level, nodes)).RetiresOnSaturation();
  }

  void RunTestCase() {
    EXPECT_CALL(mock_logger, Flush());
    page_passwords_analyser.AnalyseDocumentDOM(GetMainFrame(), &mock_logger);
  }

  PagePasswordsAnalyser page_passwords_analyser;
  MockPagePasswordsAnalyserLogger mock_logger;

 private:
  DISALLOW_COPY_AND_ASSIGN(PagePasswordsAnalyserTest);

  std::vector<blink::WebElement> elements_;
};

TEST_F(PagePasswordsAnalyserTest, PasswordFieldNotInForm) {
  LoadTestCase(kPasswordFieldNotInForm);

  Expect("Password field is not contained in a form:",
         PagePasswordsAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, PasswordFormWithoutUsernameField) {
  LoadTestCase(kPasswordFormWithoutUsernameField);

  Expect(
      "Password forms should have (optionally hidden) "
      "username fields for accessibility:",
      PagePasswordsAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, ElementsWithDuplicateIds) {
  LoadTestCase(kElementsWithDuplicateIds);

  Expect("Found 2 elements with non-unique id #duplicate:",
         PagePasswordsAnalyserLogger::kError, {0, 1});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, PasswordFormTooComplex) {
  LoadTestCase(kPasswordFormTooComplex);

  Expect(
      "Multiple forms should be contained in their own "
      "form elements; break up complex forms into ones that represent a "
      "single action:",
      PagePasswordsAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, InferredPasswordAutocompleteAttributes) {
  LoadTestCase(kInferredPasswordAutocompleteAttributes);
  size_t element_index = 0;

  // Login form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("current-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});

  // Registration form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("new-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});

  // Change password form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("current-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, InferredUsernameAutocompleteAttributes) {
  LoadTestCase(kInferredUsernameAutocompleteAttributes);
  size_t element_index = 0;

  // Login form.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.

  // Registration form.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.

  // Change password form with username.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PagePasswordsAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.

  RunTestCase();
}

}  // namespace autofill
