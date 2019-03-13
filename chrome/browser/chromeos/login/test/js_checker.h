// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_

#include <memory>
#include <string>
#include "base/strings/string_piece.h"

namespace content {
class WebContents;
}

namespace chromeos {
namespace test {

class TestConditionWaiter;

// Utility class for tests that allows us to evalute and check JavaScript
// expressions inside given web contents. All calls are made synchronously.
class JSChecker {
 public:
  JSChecker();
  explicit JSChecker(content::WebContents* web_contents);

  // Evaluates |expression|. Evaluation will be completed when this function
  // call returns.
  void Evaluate(const std::string& expression);

  // Executes |expression|. Doesn't require a correct command. Command will be
  // queued up and executed later. This function will return immediately.
  void ExecuteAsync(const std::string& expression);

  // Evaluates |expression| and returns its result.
  bool GetBool(const std::string& expression);
  int GetInt(const std::string& expression);
  std::string GetString(const std::string& expression);

  // Checks truthfulness of the given |expression|.
  void ExpectTrue(const std::string& expression);
  void ExpectFalse(const std::string& expression);

  // Compares result of |expression| with |result|.
  void ExpectEQ(const std::string& expression, int result);
  void ExpectNE(const std::string& expression, int result);
  void ExpectEQ(const std::string& expression, const std::string& result);
  void ExpectNE(const std::string& expression, const std::string& result);
  void ExpectEQ(const std::string& expression, bool result);
  void ExpectNE(const std::string& expression, bool result);

  // Checks test waiter that would await until |js_condition| evaluates
  // to true.
  std::unique_ptr<TestConditionWaiter> CreateWaiter(
      const std::string& js_condition);

  // Expects that indicated UI element is not hidden.
  void ExpectVisiblePath(std::initializer_list<base::StringPiece> element_ids);
  void ExpectVisible(const std::string& element_id);

  // Expects that indicated UI element is hidden.
  void ExpectHiddenPath(std::initializer_list<base::StringPiece> element_ids);
  void ExpectHidden(const std::string& element_id);

  // Tap on indicated UI element.
  void TapOnPath(std::initializer_list<base::StringPiece> element_ids);
  void TapOn(const std::string& element_id);

  // Types text into indicated input field. There is no single-element version
  // of method to avoid confusion.
  void TypeIntoPath(const std::string& value,
                    std::initializer_list<base::StringPiece> element_ids);

  // Selects an option in indicated <select> element. There is no single-element
  // version of method to avoid confusion.
  void SelectElementInPath(
      const std::string& value,
      std::initializer_list<base::StringPiece> element_ids);

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  void GetBoolImpl(const std::string& expression, bool* result);
  void GetIntImpl(const std::string& expression, int* result);
  void GetStringImpl(const std::string& expression, std::string* result);

  content::WebContents* web_contents_ = nullptr;
};

// Helper method to create the JSChecker instance from the login/oobe
// web-contents.
JSChecker OobeJS();

// Helper method to execute the given script in the context of OOBE.
void ExecuteOobeJS(const std::string& script);
void ExecuteOobeJSAsync(const std::string& script);

// Generates JS expression that evaluates to element in hierarchy (elements
// are searched by ID in parent). It is assumed that all intermediate elements
// are Polymer-based.
std::string GetOobeElementPath(
    std::initializer_list<base::StringPiece> element_ids);

// Helper method to create waiter over js condition that would also be satisfied
// if oobe UI is destroyed.
std::unique_ptr<TestConditionWaiter> CreatePredicateOrOobeDestroyedWaiter(
    const std::string& js_expression);

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_
