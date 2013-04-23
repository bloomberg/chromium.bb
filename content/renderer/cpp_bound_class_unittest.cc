// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for CppBoundClass, in conjunction with CppBindingExample.  Binds
// a CppBindingExample class into JavaScript in a custom test shell and tests
// the binding from the outside by loading JS into the shell.

#include "base/utf_string_conversions.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/test/render_view_test.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "webkit/glue/cpp_binding_example.h"
#include "webkit/glue/webkit_glue.h"

using webkit_glue::CppArgumentList;
using webkit_glue::CppBindingExample;
using webkit_glue::CppVariant;

namespace content {

class CppBindingExampleSubObject : public CppBindingExample {
 public:
  CppBindingExampleSubObject() {
    sub_value_.Set("sub!");
    BindProperty("sub_value", &sub_value_);
  }
 private:
  CppVariant sub_value_;
};


class CppBindingExampleWithOptionalFallback : public CppBindingExample {
 public:
  CppBindingExampleWithOptionalFallback() {
    BindProperty("sub_object", sub_object_.GetAsCppVariant());
  }

  void set_fallback_method_enabled(bool state) {
    BindFallbackCallback(state ?
        base::Bind(&CppBindingExampleWithOptionalFallback::fallbackMethod,
                   base::Unretained(this))
        : CppBoundClass::Callback());
  }

  // The fallback method does nothing, but because of it the JavaScript keeps
  // running when a nonexistent method is called on an object.
  void fallbackMethod(const CppArgumentList& args, CppVariant* result) {
  }

 private:
  CppBindingExampleSubObject sub_object_;
};

class TestObserver : public RenderViewObserver {
 public:
  explicit TestObserver(RenderView* render_view)
      : RenderViewObserver(render_view) {}
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE {
    example_bound_class_.BindToJavascript(frame, "example");
  }
  void set_fallback_method_enabled(bool use_fallback) {
    example_bound_class_.set_fallback_method_enabled(use_fallback);
  }
 private:
  CppBindingExampleWithOptionalFallback example_bound_class_;
};

class CppBoundClassTest : public RenderViewTest {
 public:
  CppBoundClassTest() {}

  virtual void SetUp() OVERRIDE {
    RenderViewTest::SetUp();
    observer_.reset(new TestObserver(view_));
    observer_->set_fallback_method_enabled(useFallback());

    WebKit::WebURLRequest url_request;
    url_request.initialize();
    url_request.setURL(GURL("about:blank"));

    GetMainFrame()->loadRequest(url_request);
    ProcessPendingMessages();
  }

  // Executes the specified JavaScript and checks that the resulting document
  // text is empty.
  void CheckJavaScriptFailure(const std::string& javascript) {
    ExecuteJavaScript(javascript.c_str());
    EXPECT_EQ("", UTF16ToASCII(webkit_glue::DumpDocumentText(GetMainFrame())));
  }

  void CheckTrue(const std::string& expression) {
    int was_page_a = -1;
    string16 check_page_a =
        ASCIIToUTF16(std::string("Number(") + expression + ")");
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
    EXPECT_EQ(1, was_page_a);
  }

 protected:
  virtual bool useFallback() {
    return false;
  }

 private:
  scoped_ptr<TestObserver> observer_;
};

class CppBoundClassWithFallbackMethodTest : public CppBoundClassTest {
 protected:
  virtual bool useFallback() OVERRIDE {
    return true;
  }
};

// Ensures that the example object has been bound to JS.
TEST_F(CppBoundClassTest, ObjectExists) {
  CheckTrue("typeof window.example == 'object'");

  // An additional check to test our test.
  CheckTrue("typeof window.invalid_object == 'undefined'");
}

TEST_F(CppBoundClassTest, PropertiesAreInitialized) {
  CheckTrue("example.my_value == 10");

  CheckTrue("example.my_other_value == 'Reinitialized!'");
}

TEST_F(CppBoundClassTest, SubOject) {
  CheckTrue("typeof window.example.sub_object == 'object'");

  CheckTrue("example.sub_object.sub_value == 'sub!'");
}

TEST_F(CppBoundClassTest, SetAndGetProperties) {
  // The property on the left will be set to the value on the right, then
  // checked to make sure it holds that same value.
  static const std::string tests[] = {
    "example.my_value", "7",
    "example.my_value", "'test'",
    "example.my_other_value", "3.14",
    "example.my_other_value", "false",
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i += 2) {
    std::string left = tests[i];
    std::string right = tests[i + 1];
    // left = right;
    std::string js = left;
    js.append(" = ");
    js.append(right);
    js.append(";");
    ExecuteJavaScript(js.c_str());
    std::string expression = left;
    expression += " == ";
    expression += right;
    CheckTrue(expression);
  }
}

TEST_F(CppBoundClassTest, SetAndGetPropertiesWithCallbacks) {
  // TODO(dglazkov): fix NPObject issues around failing property setters and
  // getters and add tests for situations when GetProperty or SetProperty fail.
  ExecuteJavaScript("example.my_value_with_callback = 10;");
  CheckTrue("example.my_value_with_callback == 10");

  ExecuteJavaScript("example.my_value_with_callback = 11;");
  CheckTrue("example.my_value_with_callback == 11");

  CheckTrue("example.same == 42");

  ExecuteJavaScript("example.same = 24;");
  CheckTrue("example.same == 42");
}

TEST_F(CppBoundClassTest, InvokeMethods) {
  // The expression on the left is expected to return the value on the right.
  static const std::string tests[] = {
    "example.echoValue(true) == true",
    "example.echoValue(13) == 13",
    "example.echoValue(2.718) == 2.718",
    "example.echoValue('yes') == 'yes'",
    "example.echoValue() == null",     // Too few arguments

    "example.echoType(false) == true",
    "example.echoType(19) == 3.14159",
    "example.echoType(9.876) == 3.14159",
    "example.echoType('test string') == 'Success!'",
    "example.echoType() == null",      // Too few arguments

    // Comparing floats that aren't integer-valued is usually problematic due
    // to rounding, but exact powers of 2 should also be safe.
    "example.plus(2.5, 18.0) == 20.5",
    "example.plus(2, 3.25) == 5.25",
    "example.plus(2, 3) == 5",
    "example.plus() == null",             // Too few arguments
    "example.plus(1) == null",            // Too few arguments
    "example.plus(1, 'test') == null",    // Wrong argument type
    "example.plus('test', 2) == null",    // Wrong argument type
    "example.plus('one', 'two') == null", // Wrong argument type
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i++)
    CheckTrue(tests[i]);

  ExecuteJavaScript("example.my_value = 3.25; example.my_other_value = 1.25;");
  CheckTrue("example.plus(example.my_value, example.my_other_value) == 4.5");
}

// Tests that invoking a nonexistent method with no fallback method stops the
// script's execution
TEST_F(CppBoundClassTest,
       InvokeNonexistentMethodNoFallback) {
  std::string js = "example.nonExistentMethod();document.writeln('SUCCESS');";
  CheckJavaScriptFailure(js);
}

// Ensures existent methods can be invoked successfully when the fallback method
// is used
TEST_F(CppBoundClassWithFallbackMethodTest,
       InvokeExistentMethodsWithFallback) {
  CheckTrue("example.echoValue(34) == 34");
}

}  // namespace content
