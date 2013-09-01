// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::string16;
using base::UTF8ToUTF16;

namespace extensions {

namespace {

const char kTestingPage[] = "/extensions/test_file.html";
const char kAnonymousFunction[] = "(anonymous function)";
const char* kBackgroundPageName =
    extensions::kGeneratedBackgroundPageFilename;
const int kNoFlags = 0;

const StackTrace& GetStackTraceFromError(const ExtensionError* error) {
  CHECK(error->type() == ExtensionError::RUNTIME_ERROR);
  return (static_cast<const RuntimeError*>(error))->stack_trace();
}

// Verify that a given |frame| has the proper source and function name.
void CheckStackFrame(const StackFrame& frame,
                     const std::string& source,
                     const std::string& function) {
  EXPECT_EQ(UTF8ToUTF16(source), frame.source);
  EXPECT_EQ(UTF8ToUTF16(function), frame.function);
}

// Verify that all properties of a given |frame| are correct. Overloaded because
// we commonly do not check line/column numbers, as they are too likely
// to change.
void CheckStackFrame(const StackFrame& frame,
                     const std::string& source,
                     const std::string& function,
                     size_t line_number,
                     size_t column_number) {
  CheckStackFrame(frame, source, function);
  EXPECT_EQ(line_number, frame.line_number);
  EXPECT_EQ(column_number, frame.column_number);
}

// Verify that all properties of a given |error| are correct.
void CheckError(const ExtensionError* error,
                ExtensionError::Type type,
                const std::string& id,
                const std::string& source,
                bool from_incognito,
                const std::string& message) {
  ASSERT_TRUE(error);
  EXPECT_EQ(type, error->type());
  EXPECT_EQ(id, error->extension_id());
  EXPECT_EQ(UTF8ToUTF16(source), error->source());
  EXPECT_EQ(from_incognito, error->from_incognito());
  EXPECT_EQ(UTF8ToUTF16(message), error->message());
}

// Verify that all properties of a JS runtime error are correct.
void CheckRuntimeError(const ExtensionError* error,
                       const std::string& id,
                       const std::string& source,
                       bool from_incognito,
                       const std::string& message,
                       logging::LogSeverity level,
                       const GURL& context,
                       size_t expected_stack_size) {
  CheckError(error,
             ExtensionError::RUNTIME_ERROR,
             id,
             source,
             from_incognito,
             message);

  const RuntimeError* runtime_error = static_cast<const RuntimeError*>(error);
  EXPECT_EQ(level, runtime_error->level());
  EXPECT_EQ(context, runtime_error->context_url());
  EXPECT_EQ(expected_stack_size, runtime_error->stack_trace().size());
}

}  // namespace

class ErrorConsoleBrowserTest : public ExtensionBrowserTest {
 public:
  ErrorConsoleBrowserTest() : error_console_(NULL) { }
  virtual ~ErrorConsoleBrowserTest() { }

 protected:
  // A helper class in order to wait for the proper number of errors to be
  // caught by the ErrorConsole. This will run the MessageLoop until a given
  // number of errors are observed.
  // Usage:
  //   ...
  //   ErrorObserver observer(3, error_console);
  //   <Cause three errors...>
  //   observer.WaitForErrors();
  //   <Perform any additional checks...>
  class ErrorObserver : public ErrorConsole::Observer {
   public:
    ErrorObserver(size_t errors_expected, ErrorConsole* error_console)
        : errors_observed_(0),
          errors_expected_(errors_expected),
          waiting_(false),
          error_console_(error_console) {
      error_console_->AddObserver(this);
    }
    virtual ~ErrorObserver() {
      if (error_console_)
        error_console_->RemoveObserver(this);
    }

    // ErrorConsole::Observer implementation.
    virtual void OnErrorAdded(const ExtensionError* error) OVERRIDE {
      ++errors_observed_;
      if (errors_observed_ >= errors_expected_) {
        if (waiting_)
          base::MessageLoopForUI::current()->Quit();
      }
    }

    virtual void OnErrorConsoleDestroyed() OVERRIDE {
      error_console_ = NULL;
    }

    // Spin until the appropriate number of errors have been observed.
    void WaitForErrors() {
      if (errors_observed_ < errors_expected_) {
        waiting_ = true;
        content::RunMessageLoop();
        waiting_ = false;
      }
    }

   private:
    size_t errors_observed_;
    size_t errors_expected_;
    bool waiting_;

    ErrorConsole* error_console_;

    DISALLOW_COPY_AND_ASSIGN(ErrorObserver);
  };

  // The type of action which we take after we load an extension in order to
  // cause any errors.
  enum Action {
    ACTION_NAVIGATE,  // navigate to a page to allow a content script to run.
    ACTION_BROWSER_ACTION,  // simulate a browser action click.
    ACTION_NONE  // Do nothing (errors will be caused by a background script,
                 // or by a manifest/loading warning).
  };

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    // We need to enable the ErrorConsole FeatureSwitch in order to collect
    // errors.
    FeatureSwitch::error_console()->SetOverrideValue(
        FeatureSwitch::OVERRIDE_ENABLED);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Errors are only kept if we have Developer Mode enabled.
    profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

    error_console_ = ErrorConsole::Get(profile());
    CHECK(error_console_);

    test_data_dir_ = test_data_dir_.AppendASCII("error_console");
  }

  const GURL& GetTestURL() {
    if (test_url_.is_empty()) {
      CHECK(embedded_test_server()->InitializeAndWaitUntilReady());
      test_url_ = embedded_test_server()->GetURL(kTestingPage);
    }
    return test_url_;
  }

  // Load the extension at |path|, take the specified |action|, and wait for
  // |expected_errors| errors. Populate |extension| with a pointer to the loaded
  // extension.
  void LoadExtensionAndCheckErrors(
      const std::string& path,
      int flags,
      size_t errors_expected,
      Action action,
      const Extension** extension) {
    ErrorObserver observer(errors_expected, error_console_);
    *extension =
        LoadExtensionWithFlags(test_data_dir_.AppendASCII(path), flags);
    ASSERT_TRUE(*extension);

    switch (action) {
      case ACTION_NAVIGATE: {
        ui_test_utils::NavigateToURL(browser(), GetTestURL());
        break;
      }
      case ACTION_BROWSER_ACTION: {
        ExtensionService* service =
            extensions::ExtensionSystem::Get(profile())->extension_service();
        service->toolbar_model()->ExecuteBrowserAction(
            *extension, browser(), NULL);
        break;
      }
      case ACTION_NONE:
        break;
      default:
        NOTREACHED();
    }

    observer.WaitForErrors();

    // We should only have errors for a single extension, or should have no
    // entries, if no errors were expected.
    ASSERT_EQ(errors_expected > 0 ? 1u : 0u, error_console()->errors().size());
    ASSERT_EQ(
        errors_expected,
        error_console()->GetErrorsForExtension((*extension)->id()).size());
  }

  ErrorConsole* error_console() { return error_console_; }
 private:
  // The URL used in testing for simple page navigations.
  GURL test_url_;

  // Weak reference to the ErrorConsole.
  ErrorConsole* error_console_;
};

// Load an extension which, upon visiting any page, first sends out a console
// log, and then crashes with a JS TypeError.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest,
                       ContentScriptLogAndRuntimeError) {
  const Extension* extension = NULL;
  LoadExtensionAndCheckErrors(
      "content_script_log_and_runtime_error",
      kNoFlags,
      2u,  // Two errors: A log message and a JS type error.
      ACTION_NAVIGATE,
      &extension);

  std::string script_url = extension->url().Resolve("content_script.js").spec();

  const ErrorConsole::ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  // The first error should be a console log.
  CheckRuntimeError(errors[0],
                    extension->id(),
                    script_url,  // The source should be the content script url.
                    false,  // Not from incognito.
                    "Hello, World!",  // The error message is the log.
                    logging::LOG_INFO,
                    GetTestURL(),  // Content scripts run in the web page.
                    2u);

  const StackTrace& stack_trace1 = GetStackTraceFromError(errors[0]);
  CheckStackFrame(stack_trace1[0],
                  script_url,
                  "logHelloWorld",  // function name
                  6u,  // line number
                  11u /* column number */ );

  CheckStackFrame(stack_trace1[1],
                  script_url,
                  kAnonymousFunction,
                  9u,
                  1u);

  // The second error should be a runtime error.
  CheckRuntimeError(errors[1],
                    extension->id(),
                    script_url,
                    false,  // not from incognito
                    "Uncaught TypeError: "
                        "Cannot set property 'foo' of undefined",
                    logging::LOG_ERROR,  // JS errors are always ERROR level.
                    GetTestURL(),
                    1u);

  const StackTrace& stack_trace2 = GetStackTraceFromError(errors[1]);
  CheckStackFrame(stack_trace2[0],
                  script_url,
                  kAnonymousFunction,
                  12u,
                  1u);
}

// Catch an error from a BrowserAction; this is more complex than a content
// script error, since browser actions are routed through our own code.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, BrowserActionRuntimeError) {
  const Extension* extension = NULL;
  LoadExtensionAndCheckErrors(
      "browser_action_runtime_error",
      kNoFlags,
      1u,  // One error: A reference error from within the browser action.
      ACTION_BROWSER_ACTION,
      &extension);

  std::string event_bindings_str = "event_bindings";
  std::string script_url = extension->url().Resolve("browser_action.js").spec();

  const ErrorConsole::ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  CheckRuntimeError(
      errors[0],
      extension->id(),
      script_url,
      false,  // not incognito
      "Error in event handler for browserAction.onClicked: "
          "ReferenceError: baz is not defined",
      logging::LOG_ERROR,
      extension->url().Resolve(kBackgroundPageName),
      6u);

  const StackTrace& stack_trace = GetStackTraceFromError(errors[0]);

  CheckStackFrame(stack_trace[0], script_url, kAnonymousFunction);
  CheckStackFrame(stack_trace[1],
                  "extensions::SafeBuiltins",
                  std::string("Function.target.") + kAnonymousFunction);
  CheckStackFrame(
      stack_trace[2], event_bindings_str, "Event.dispatchToListener");
  CheckStackFrame(stack_trace[3], event_bindings_str, "Event.dispatch_");
  CheckStackFrame(stack_trace[4], event_bindings_str, "dispatchArgs");
  CheckStackFrame(stack_trace[5], event_bindings_str, "dispatchEvent");
}

}  // namespace extensions
