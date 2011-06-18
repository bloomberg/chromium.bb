// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/web_ui_browsertest.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/test_chrome_web_ui_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const FilePath::CharType kWebUILibraryJS[] = FILE_PATH_LITERAL("test_api.js");
const FilePath::CharType kWebUITestFolder[] = FILE_PATH_LITERAL("webui");
base::LazyInstance<std::vector<std::string> > error_messages_(
    base::LINKER_INITIALIZED);

// Intercepts all log messages.
bool LogHandler(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) {
  if (severity == logging::LOG_ERROR)
    error_messages_.Get().push_back(str);

  return false;
}

} // namespace

WebUIBrowserTest::~WebUIBrowserTest() {}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name) {
  return RunJavascriptFunction(function_name, ConstValueVector());
}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name,
                                             const Value& arg) {
  ConstValueVector args;
  args.push_back(&arg);
  return RunJavascriptFunction(function_name, args);
}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name,
                                             const Value& arg1,
                                             const Value& arg2) {
  ConstValueVector args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  return RunJavascriptFunction(function_name, args);
}

bool WebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name,
    const ConstValueVector& function_arguments) {
  return RunJavascriptUsingHandler(function_name, function_arguments, false);
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name) {
  return RunJavascriptTest(test_name, ConstValueVector());
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                         const Value& arg) {
  ConstValueVector args;
  args.push_back(&arg);
  return RunJavascriptTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                         const Value& arg1,
                                         const Value& arg2) {
  ConstValueVector args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  return RunJavascriptTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptTest(
    const std::string& test_name,
    const ConstValueVector& test_arguments) {
  return RunJavascriptUsingHandler(test_name, test_arguments, true);
}

WebUIBrowserTest::WebUIBrowserTest()
    : test_handler_(new WebUITestHandler()) {}

void WebUIBrowserTest::SetUpInProcessBrowserTestFixture() {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_));
  test_data_directory_ = test_data_directory_.Append(kWebUITestFolder);

  // TODO(dtseng): should this be part of every BrowserTest or just WebUI test.
  FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::AddDataPackToSharedInstance(resources_pack_path);

  AddLibrary(FilePath(kWebUILibraryJS));
}

WebUIMessageHandler* WebUIBrowserTest::GetMockMessageHandler() {
  return NULL;
}

void WebUIBrowserTest::BuildJavascriptLibraries(std::string* content) {
  ASSERT_TRUE(content != NULL);
  std::string library_content, src_content;

  std::vector<FilePath>::iterator user_libraries_iterator;
  for (user_libraries_iterator = user_libraries.begin();
       user_libraries_iterator != user_libraries.end();
       ++user_libraries_iterator) {
    if (user_libraries_iterator->IsAbsolute()) {
      ASSERT_TRUE(file_util::ReadFileToString(*user_libraries_iterator,
                                              &library_content));
    } else {
      ASSERT_TRUE(file_util::ReadFileToString(
          test_data_directory_.Append(*user_libraries_iterator),
              &library_content));
    }
    content->append(library_content);
    content->append(";\n");
  }
}

string16 WebUIBrowserTest::BuildRunTestJSCall(
    const std::string& function_name,
    const WebUIBrowserTest::ConstValueVector& test_func_args) {
  WebUIBrowserTest::ConstValueVector arguments;
  StringValue function_name_arg(function_name);
  arguments.push_back(&function_name_arg);
  ListValue baked_argument_list;
  WebUIBrowserTest::ConstValueVector::const_iterator arguments_iterator;
  for (arguments_iterator = test_func_args.begin();
       arguments_iterator != test_func_args.end();
       ++arguments_iterator) {
    baked_argument_list.Append((Value *)*arguments_iterator);
  }
  arguments.push_back(&baked_argument_list);
  return WebUI::GetJavascriptCall(std::string("runTest"), arguments);
}

bool WebUIBrowserTest::RunJavascriptUsingHandler(
    const std::string& function_name,
    const ConstValueVector& function_arguments,
    bool is_test) {
  std::string content;
  BuildJavascriptLibraries(&content);

  if (!function_name.empty()) {
    string16 called_function;
    if (is_test) {
      called_function = BuildRunTestJSCall(function_name, function_arguments);
    } else {
      called_function = WebUI::GetJavascriptCall(function_name,
                                                 function_arguments);
    }
    content.append(UTF16ToUTF8(called_function));
  }
  SetupHandlers();
  logging::SetLogMessageHandler(&LogHandler);
  bool result = test_handler_->RunJavascript(content, is_test);
  logging::SetLogMessageHandler(NULL);

  if (error_messages_.Get().size() > 0) {
    LOG(ERROR) << "Encountered javascript console error(s)";
    result = false;
    error_messages_.Get().clear();
  }
  return result;
}

void WebUIBrowserTest::SetupHandlers() {
  WebUI* web_ui_instance =
      browser()->GetSelectedTabContents()->web_ui();
  ASSERT_TRUE(web_ui_instance != NULL);
  web_ui_instance->register_callback_overwrites(true);
  test_handler_->Attach(web_ui_instance);

  if (GetMockMessageHandler())
    GetMockMessageHandler()->Attach(web_ui_instance);
}

void WebUIBrowserTest::AddLibrary(const FilePath& library_path) {
  user_libraries.push_back(library_path);
}

IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, TestSamplePass) {
  AddLibrary(FilePath(FILE_PATH_LITERAL("sample_downloads.js")));

  // Navigate to UI.
  // TODO(dtseng): make accessor for subclasses to return?
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));

  ASSERT_TRUE(RunJavascriptTest("testAssertFalse"));
  ASSERT_FALSE(RunJavascriptTest("FAILS_testAssertFalse"));
  ASSERT_TRUE(RunJavascriptTest("testInitialFocus"));
  ASSERT_FALSE(RunJavascriptTest("testConsoleError"));
}

// According to the interface for EXPECT_FATAL_FAILURE
// (http://code.google.com/p/googletest/wiki/AdvancedGuide#Catching_Failures)
// the statement must be statically available. Therefore, we make a static
// global s_test_ which should point to |this| for the duration of the test run
// and be cleared afterward.
class WebUIBrowserExpectFailTest : public WebUIBrowserTest {
 public:
  WebUIBrowserExpectFailTest() {
    EXPECT_FALSE(s_test_);
    s_test_ = this;
  }

 protected:
  virtual ~WebUIBrowserExpectFailTest() {
    EXPECT_TRUE(s_test_);
    s_test_ = NULL;
  }

  static void RunJavascriptTestNoReturn(const std::string& testname) {
    EXPECT_TRUE(s_test_);
    s_test_->RunJavascriptTest(testname);
  }

 private:
  static WebUIBrowserTest* s_test_;
};
WebUIBrowserTest* WebUIBrowserExpectFailTest::s_test_ = NULL;

IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestFailsFast) {
  AddLibrary(FilePath(FILE_PATH_LITERAL("sample_downloads.js")));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));
  EXPECT_FATAL_FAILURE(RunJavascriptTestNoReturn("FAILS_BogusFunctionName"),
                       "WebUITestHandler::Observe");
}


// This test framework is used in the generated tests, which are included
// below. WebUIBrowserTest requires being on a page which is a WebUI page. Using
// the TestChromeWebUIFactory, we use a dummy URL |kChromeTestBrowserTestPass|,
// which we force to be a WebUI page.
class WebUIBrowserTestPass
    : public WebUIBrowserTest,
      public TestChromeWebUIFactory::WebUIProvider {
 private:
  // TestChromeWebUIFactory::WebUIProvider:
  virtual WebUI* NewWebUI(TabContents* tab_contents,
                          const GURL& url) OVERRIDE {
    return new WebUI(tab_contents);
  }

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    ui_test_utils::NavigateToURL(browser(),
                                 GURL(kChromeTestBrowserTestPass));
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
    TestChromeWebUIFactory::AddFactoryOverride(
        GURL(kChromeTestBrowserTestPass).host(), this);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    WebUIBrowserTest::TearDownInProcessBrowserTestFixture();
    TestChromeWebUIFactory::RemoveFactoryOverride(
        GURL(kChromeTestBrowserTestPass).host());
  }

  static const char kChromeTestBrowserTestPass[];
};

const char WebUIBrowserTestPass::kChromeTestBrowserTestPass[] =
    "chrome://WebUIBrowserTestPass";

#include "chrome/browser/ui/webui/web_ui_browsertest-inl.h"
