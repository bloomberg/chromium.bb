// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/web_ui_browsertest.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/test_chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_tab_strip_model_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;
using content::WebUIController;
using content::WebUIMessageHandler;

namespace {

const FilePath::CharType kMockJS[] = FILE_PATH_LITERAL("mock4js.js");
const FilePath::CharType kWebUILibraryJS[] = FILE_PATH_LITERAL("test_api.js");
const FilePath::CharType kWebUITestFolder[] = FILE_PATH_LITERAL("webui");
base::LazyInstance<std::vector<std::string> > error_messages_ =
    LAZY_INSTANCE_INITIALIZER;

// Intercepts all log messages.
bool LogHandler(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) {
  if (severity == logging::LOG_ERROR &&
      file &&
      std::string("CONSOLE") == file) {
    error_messages_.Get().push_back(str);
  }

  return false;
}

}  // namespace

WebUIBrowserTest::~WebUIBrowserTest() {}

void WebUIBrowserTest::AddLibrary(const FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name) {
  return RunJavascriptFunction(function_name, ConstValueVector());
}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name,
                                             Value* arg) {
  ConstValueVector args;
  args.push_back(arg);
  return RunJavascriptFunction(function_name, args);
}

bool WebUIBrowserTest::RunJavascriptFunction(const std::string& function_name,
                                             Value* arg1,
                                             Value* arg2) {
  ConstValueVector args;
  args.push_back(arg1);
  args.push_back(arg2);
  return RunJavascriptFunction(function_name, args);
}

bool WebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name,
    const ConstValueVector& function_arguments) {
  return RunJavascriptUsingHandler(
      function_name, function_arguments, false, false, NULL);
}

bool WebUIBrowserTest::RunJavascriptTestF(bool is_async,
                                          const std::string& test_fixture,
                                          const std::string& test_name) {
  ConstValueVector args;
  args.push_back(Value::CreateStringValue(test_fixture));
  args.push_back(Value::CreateStringValue(test_name));

  if (is_async)
    return RunJavascriptAsyncTest("RUN_TEST_F", args);
  else
    return RunJavascriptTest("RUN_TEST_F", args);
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name) {
  return RunJavascriptTest(test_name, ConstValueVector());
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                         Value* arg) {
  ConstValueVector args;
  args.push_back(arg);
  return RunJavascriptTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                         Value* arg1,
                                         Value* arg2) {
  ConstValueVector args;
  args.push_back(arg1);
  args.push_back(arg2);
  return RunJavascriptTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptTest(
    const std::string& test_name,
    const ConstValueVector& test_arguments) {
  return RunJavascriptUsingHandler(
      test_name, test_arguments, true, false, NULL);
}

bool WebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name) {
  return RunJavascriptAsyncTest(test_name, ConstValueVector());
}

bool WebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                              Value* arg) {
  ConstValueVector args;
  args.push_back(arg);
  return RunJavascriptAsyncTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                              Value* arg1,
                                              Value* arg2) {
  ConstValueVector args;
  args.push_back(arg1);
  args.push_back(arg2);
  return RunJavascriptAsyncTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                              Value* arg1,
                                              Value* arg2,
                                              Value* arg3) {
  ConstValueVector args;
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  return RunJavascriptAsyncTest(test_name, args);
}

bool WebUIBrowserTest::RunJavascriptAsyncTest(
    const std::string& test_name,
    const ConstValueVector& test_arguments) {
  return RunJavascriptUsingHandler(test_name, test_arguments, true, true, NULL);
}

void WebUIBrowserTest::PreLoadJavascriptLibraries(
    const std::string& preload_test_fixture,
    const std::string& preload_test_name,
    RenderViewHost* preload_host) {
  ASSERT_FALSE(libraries_preloaded_);
  ConstValueVector args;
  args.push_back(Value::CreateStringValue(preload_test_fixture));
  args.push_back(Value::CreateStringValue(preload_test_name));
  RunJavascriptUsingHandler(
      "preloadJavascriptLibraries", args, false, false, preload_host);
  libraries_preloaded_ = true;
}

void WebUIBrowserTest::BrowsePreload(const GURL& browse_to) {
  TestNavigationObserver navigation_observer(
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()),
      this, 1);
  browser::NavigateParams params(
      browser(), GURL(browse_to), content::PAGE_TRANSITION_TYPED);
  params.disposition = CURRENT_TAB;
  browser::Navigate(&params);
  navigation_observer.WaitForObservation(
      base::Bind(&ui_test_utils::RunMessageLoop),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoopForUI::current())));

}

void WebUIBrowserTest::BrowsePrintPreload(const GURL& browse_to) {
  ui_test_utils::NavigateToURL(browser(), browse_to);

  TestTabStripModelObserver tabstrip_observer(
      browser()->tab_strip_model(), this);
  browser()->Print();
  tabstrip_observer.WaitForObservation(
      base::Bind(&ui_test_utils::RunMessageLoop),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoopForUI::current())));

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);
  TabContentsWrapper* preview_tab = tab_controller->GetPrintPreviewForTab(
      browser()->GetSelectedTabContentsWrapper());
  ASSERT_TRUE(preview_tab);
  SetWebUIInstance(preview_tab->web_contents()->GetWebUI());
}

const char WebUIBrowserTest::kDummyURL[] = "chrome://DummyURL";

WebUIBrowserTest::WebUIBrowserTest()
    : test_handler_(new WebUITestHandler()),
      libraries_preloaded_(false),
      override_selected_web_ui_(NULL) {}

void WebUIBrowserTest::set_preload_test_fixture(
    const std::string& preload_test_fixture) {
  preload_test_fixture_ = preload_test_fixture;
}

void WebUIBrowserTest::set_preload_test_name(
    const std::string& preload_test_name) {
  preload_test_name_ = preload_test_name;
}

namespace {

// DataSource for the dummy URL.  If no data source is provided then an error
// page is shown. While this doesn't matter for most tests, without it,
// navigation to different anchors cannot be listened to (via the hashchange
// event).
class MockWebUIDataSource : public ChromeURLDataManager::DataSource {
 public:
  MockWebUIDataSource()
      : ChromeURLDataManager::DataSource("dummyurl", MessageLoop::current()) {}

 private:
  virtual ~MockWebUIDataSource() {}

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE {
    std::string dummy_html = "<html><body>Dummy</body></html>";
    scoped_refptr<base::RefCountedString> response =
        base::RefCountedString::TakeString(&dummy_html);
    SendResponse(request_id, response);
  }

  std::string GetMimeType(const std::string& path) const OVERRIDE {
    return "text/html";
  }

  DISALLOW_COPY_AND_ASSIGN(MockWebUIDataSource);
};

// WebUIProvider to allow attaching the DataSource for the dummy URL when
// testing.
class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MockWebUIProvider() {}

  // Returns a new WebUI
  WebUIController* NewWebUI(content::WebUI* web_ui, const GURL& url) OVERRIDE {
    WebUIController* controller = new content::WebUIController(web_ui);
    Profile* profile = Profile::FromWebUI(web_ui);
    ChromeURLDataManager::AddDataSource(profile, new MockWebUIDataSource());
    return controller;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebUIProvider);
};

base::LazyInstance<MockWebUIProvider> mock_provider_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void WebUIBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  logging::SetLogMessageHandler(&LogHandler);
}

void WebUIBrowserTest::CleanUpOnMainThread() {
  InProcessBrowserTest::CleanUpOnMainThread();

  logging::SetLogMessageHandler(NULL);
}

void WebUIBrowserTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  TestChromeWebUIControllerFactory::AddFactoryOverride(
      GURL(kDummyURL).host(), mock_provider_.Pointer());

  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_));
  test_data_directory_ = test_data_directory_.Append(kWebUITestFolder);
  ASSERT_TRUE(PathService::Get(chrome::DIR_GEN_TEST_DATA,
                               &gen_test_data_directory_));

  // TODO(dtseng): should this be part of every BrowserTest or just WebUI test.
  FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPack(
      resources_pack_path, ui::ResourceHandle::kScaleFactor100x);

  FilePath mockPath;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &mockPath));
  mockPath = mockPath.AppendASCII("chrome");
  mockPath = mockPath.AppendASCII("third_party");
  mockPath = mockPath.AppendASCII("mock4js");
  mockPath = mockPath.Append(kMockJS);
  AddLibrary(mockPath);
  AddLibrary(FilePath(kWebUILibraryJS));
}

void WebUIBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  TestChromeWebUIControllerFactory::RemoveFactoryOverride(
      GURL(kDummyURL).host());
}

void WebUIBrowserTest::SetWebUIInstance(content::WebUI* web_ui) {
  override_selected_web_ui_ = web_ui;
}

WebUIMessageHandler* WebUIBrowserTest::GetMockMessageHandler() {
  return NULL;
}

GURL WebUIBrowserTest::WebUITestDataPathToURL(
    const FilePath::StringType& path) {
  FilePath dir_test_data;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data));
  FilePath test_path(dir_test_data.Append(kWebUITestFolder).Append(path));
  EXPECT_TRUE(file_util::PathExists(test_path));
  return net::FilePathToFileURL(test_path);
}

void WebUIBrowserTest::OnJsInjectionReady(RenderViewHost* render_view_host) {
  PreLoadJavascriptLibraries(preload_test_fixture_, preload_test_name_,
                             render_view_host);
}

void WebUIBrowserTest::BuildJavascriptLibraries(string16* content) {
  ASSERT_TRUE(content != NULL);
  std::string utf8_content;
  std::vector<FilePath>::iterator user_libraries_iterator;
  for (user_libraries_iterator = user_libraries_.begin();
       user_libraries_iterator != user_libraries_.end();
       ++user_libraries_iterator) {
    std::string library_content;
    if (user_libraries_iterator->IsAbsolute()) {
      ASSERT_TRUE(file_util::ReadFileToString(*user_libraries_iterator,
                                              &library_content))
          << user_libraries_iterator->value();
    } else {
      bool ok = file_util::ReadFileToString(
          gen_test_data_directory_.Append(*user_libraries_iterator),
          &library_content);
      if (!ok) {
        ok = file_util::ReadFileToString(
            test_data_directory_.Append(*user_libraries_iterator),
            &library_content);
      }
      ASSERT_TRUE(ok) << user_libraries_iterator->value();
    }
    utf8_content.append(library_content);
    utf8_content.append(";\n");
  }
  content->append(UTF8ToUTF16(utf8_content));
}

string16 WebUIBrowserTest::BuildRunTestJSCall(
    bool is_async,
    const std::string& function_name,
    const WebUIBrowserTest::ConstValueVector& test_func_args) {
  WebUIBrowserTest::ConstValueVector arguments;
  base::FundamentalValue is_async_arg(is_async);
  arguments.push_back(&is_async_arg);
  StringValue function_name_arg(function_name);
  arguments.push_back(&function_name_arg);
  ListValue baked_argument_list;
  WebUIBrowserTest::ConstValueVector::const_iterator arguments_iterator;
  for (arguments_iterator = test_func_args.begin();
       arguments_iterator != test_func_args.end();
       ++arguments_iterator) {
    baked_argument_list.Append(const_cast<Value*>(*arguments_iterator));
  }
  arguments.push_back(&baked_argument_list);
  return content::WebUI::GetJavascriptCall(std::string("runTest"), arguments);
}

bool WebUIBrowserTest::RunJavascriptUsingHandler(
    const std::string& function_name,
    const ConstValueVector& function_arguments,
    bool is_test,
    bool is_async,
    RenderViewHost* preload_host) {

  string16 content;
  if (!libraries_preloaded_)
    BuildJavascriptLibraries(&content);

  if (!function_name.empty()) {
    string16 called_function;
    if (is_test) {
      called_function = BuildRunTestJSCall(
          is_async, function_name, function_arguments);
    } else {
      called_function = content::WebUI::GetJavascriptCall(function_name,
                                                          function_arguments);
    }
    content.append(called_function);
  }

  if (!preload_host)
    SetupHandlers();

  bool result = true;

  if (is_test)
    result = test_handler_->RunJavaScriptTestWithResult(content);
  else if (preload_host)
    test_handler_->PreloadJavaScript(content, preload_host);
  else
    test_handler_->RunJavaScript(content);

  if (error_messages_.Get().size() > 0) {
    LOG(ERROR) << "Encountered javascript console error(s)";
    result = false;
    error_messages_.Get().clear();
  }
  return result;
}

void WebUIBrowserTest::SetupHandlers() {
  content::WebUI* web_ui_instance = override_selected_web_ui_ ?
      override_selected_web_ui_ :
      browser()->GetSelectedWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui_instance != NULL);

  test_handler_->set_web_ui(web_ui_instance);
  test_handler_->RegisterMessages();

  if (GetMockMessageHandler()) {
    GetMockMessageHandler()->set_web_ui(web_ui_instance);
    GetMockMessageHandler()->RegisterMessages();
  }
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

  static void RunJavascriptAsyncTestNoReturn(const std::string& testname) {
    EXPECT_TRUE(s_test_);
    s_test_->RunJavascriptAsyncTest(testname);
  }

 private:
  static WebUIBrowserTest* s_test_;
};

WebUIBrowserTest* WebUIBrowserExpectFailTest::s_test_ = NULL;

// Test that bogus javascript fails fast - no timeout waiting for result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestFailsFast) {
  AddLibrary(FilePath(FILE_PATH_LITERAL("sample_downloads.js")));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));
  EXPECT_FATAL_FAILURE(RunJavascriptTestNoReturn("FAILS_BogusFunctionName"),
                       "WebUITestHandler::Observe");
}

// Test that bogus javascript fails fast - no timeout waiting for result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestRuntimeErrorFailsFast) {
  AddLibrary(FilePath(FILE_PATH_LITERAL("runtime_error.js")));
  ui_test_utils::NavigateToURL(browser(), GURL(kDummyURL));
  EXPECT_FATAL_FAILURE(RunJavascriptTestNoReturn("TestRuntimeErrorFailsFast"),
                       "WebUITestHandler::Observe");
}

// Test that bogus javascript fails async test fast as well - no timeout waiting
// for result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestFailsAsyncFast) {
  AddLibrary(FilePath(FILE_PATH_LITERAL("sample_downloads.js")));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));
  EXPECT_FATAL_FAILURE(
      RunJavascriptAsyncTestNoReturn("FAILS_BogusFunctionName"),
      "WebUITestHandler::Observe");
}

// Tests that the async framework works.
class WebUIBrowserAsyncTest : public WebUIBrowserTest {
 public:
  // Calls the testDone() function from test_api.js
  void TestDone() {
    RunJavascriptFunction("testDone");
  }

  // Starts a failing test.
  void RunTestFailsAssert() {
    RunJavascriptFunction("runAsync",
                          Value::CreateStringValue("testFailsAssert"));
  }

  // Starts a passing test.
  void RunTestPasses() {
    RunJavascriptFunction("runAsync", Value::CreateStringValue("testPasses"));
  }

 protected:
  WebUIBrowserAsyncTest() {}

  // Class to synchronize asynchronous javascript activity with the tests.
  class AsyncWebUIMessageHandler : public WebUIMessageHandler {
   public:
    AsyncWebUIMessageHandler() {}

    MOCK_METHOD1(HandleTestContinues, void(const ListValue*));
    MOCK_METHOD1(HandleTestFails, void(const ListValue*));
    MOCK_METHOD1(HandleTestPasses, void(const ListValue*));

   private:
    virtual void RegisterMessages() OVERRIDE {
      web_ui()->RegisterMessageCallback("startAsyncTest",
          base::Bind(&AsyncWebUIMessageHandler::HandleStartAsyncTest,
                     base::Unretained(this)));
      web_ui()->RegisterMessageCallback("testContinues",
          base::Bind(&AsyncWebUIMessageHandler::HandleTestContinues,
                     base::Unretained(this)));
      web_ui()->RegisterMessageCallback("testFails",
          base::Bind(&AsyncWebUIMessageHandler::HandleTestFails,
                     base::Unretained(this)));
      web_ui()->RegisterMessageCallback("testPasses",
          base::Bind(&AsyncWebUIMessageHandler::HandleTestPasses,
                     base::Unretained(this)));
    }

    // Starts the test in |list_value|[0] with the runAsync wrapper.
    void HandleStartAsyncTest(const ListValue* list_value) {
      Value* test_name;
      ASSERT_TRUE(list_value->Get(0, &test_name));
      web_ui()->CallJavascriptFunction("runAsync", *test_name);
    }

    DISALLOW_COPY_AND_ASSIGN(AsyncWebUIMessageHandler);
  };

  // Handler for this object.
  ::testing::StrictMock<AsyncWebUIMessageHandler> message_handler_;

 private:
  // Provide this object's handler.
  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE {
    return &message_handler_;
  }

  // Set up and browse to kDummyURL for all tests.
  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(FilePath(FILE_PATH_LITERAL("async.js")));
    ui_test_utils::NavigateToURL(browser(), GURL(kDummyURL));
  }

  DISALLOW_COPY_AND_ASSIGN(WebUIBrowserAsyncTest);
};

// Test that assertions fail immediately after assertion fails (no testContinues
// message). (Sync version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestSyncOkTestFail) {
  ASSERT_FALSE(RunJavascriptTest("testFailsAssert"));
}

// Test that assertions fail immediately after assertion fails (no testContinues
// message). (Async version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncFailsAssert) {
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));
  ASSERT_FALSE(RunJavascriptAsyncTest(
      "startAsyncTest", Value::CreateStringValue("testFailsAssert")));
}

// Test that expectations continue the function, but fail the test.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncFailsExpect) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));
  ASSERT_FALSE(RunJavascriptAsyncTest(
      "startAsyncTest", Value::CreateStringValue("testFailsExpect")));
}

// Test that test continues and passes. (Sync version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestSyncPasses) {
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  ASSERT_TRUE(RunJavascriptTest("testPasses"));
}

// Test that test continues and passes. (Async version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPasses) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::TestDone));
  ASSERT_TRUE(RunJavascriptAsyncTest(
      "startAsyncTest", Value::CreateStringValue("testPasses")));
}

// Test that two tests pass.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPassPass) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::RunTestPasses));
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::TestDone));
  ASSERT_TRUE(RunJavascriptAsyncTest(
      "startAsyncTest", Value::CreateStringValue("testPasses")));
}

// Test that first test passes; second fails.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPassThenFail) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::RunTestFailsAssert));
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));
  ASSERT_FALSE(RunJavascriptAsyncTest(
      "startAsyncTest", Value::CreateStringValue("testPasses")));
}

// Test that testDone() with failure first then sync pass still fails.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncDoneFailFirstSyncPass) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));

  // Call runAsync directly instead of deferring through startAsyncTest. It will
  // call testDone() on failure, then return.
  ASSERT_FALSE(RunJavascriptAsyncTest(
      "runAsync", Value::CreateStringValue("testAsyncDoneFailFirstSyncPass")));
}

// Test that calling testDone during RunJavascriptAsyncTest still completes
// when waiting for async result. This is similar to the previous test, but call
// testDone directly and expect pass result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestTestDoneEarlyPassesAsync) {
  ASSERT_TRUE(RunJavascriptAsyncTest("testDone"));
}

// Test that calling testDone during RunJavascriptTest still completes when
// waiting for async result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestTestDoneEarlyPasses) {
  ASSERT_TRUE(RunJavascriptTest("testDone"));
}
