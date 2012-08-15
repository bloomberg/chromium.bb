// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/timer.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

// Base class for handling a stream of automation messages produced by a
// JavascriptTestObserver.
class TestMessageHandler {
 public:
  enum MessageResponse {
    // Reset the timeout and keep running.
    CONTINUE,
    // Stop runnning.
    DONE
  };

  TestMessageHandler() : ok_(true) {}
  virtual ~TestMessageHandler() {};

  virtual MessageResponse HandleMessage(const std::string& json) = 0;

  void SetError(const std::string& message) {
    ok_ = false;
    error_message_ = message;
  }

  bool ok() const {
    return ok_;
  }

  const std::string& error_message() const {
    return error_message_;
  }

 private:
  bool ok_;
  std::string error_message_;
};

// A helper base class that decodes structured automation messages of the form:
// {"type": type_name, ...}
class StructuredMessageHandler : public TestMessageHandler {
 public:
  virtual MessageResponse HandleMessage(const std::string& json) OVERRIDE {
    scoped_ptr<Value> value;
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    // Automation messages are stringified before they are sent because the
    // automation channel cannot handle arbitrary objects.  This means we
    // need to decode the json twice to get the original message.
    value.reset(reader.ReadToValue(json));
    if (!value.get())
      return InternalError("Could parse automation JSON: " + json +
                           " because " + reader.GetErrorMessage());

    std::string temp;
    if (!value->GetAsString(&temp))
      return InternalError("Message was not a string: " + json);

    value.reset(reader.ReadToValue(temp));
    if (!value.get())
      return InternalError("Could not parse message JSON: " + temp +
                           " because " + reader.GetErrorMessage());

    DictionaryValue* msg;
    if (!value->GetAsDictionary(&msg))
      return InternalError("Message was not an object: " + temp);

    std::string type;
    if (!msg->GetString("type", &type))
      return MissingField("unknown", "type");

    return HandleStructuredMessage(type, msg);
  }

  virtual MessageResponse HandleStructuredMessage(const std::string& type,
                                                  DictionaryValue* msg) = 0;

  MessageResponse MissingField(const std::string& type,
                               const std::string& field) WARN_UNUSED_RESULT {
    return InternalError(type + " message did not have field: " + field);
  }

  MessageResponse InternalError(const std::string& reason) WARN_UNUSED_RESULT {
    SetError(reason);
    return DONE;
  }
};

// A simple structured message handler for tests that load nexes.
class LoadTestMessageHandler : public StructuredMessageHandler {
 public:
   LoadTestMessageHandler() : test_passed_(false) {}

  void Log(const std::string& type, const std::string& message) {
    // TODO(ncbray) better logging.
    LOG(INFO) << type << " " << message;
  }

  virtual MessageResponse HandleStructuredMessage(
      const std::string& type,
      DictionaryValue* msg) OVERRIDE {
    if (type == "Log") {
      std::string message;
      if (!msg->GetString("message", &message))
        return MissingField(type, "message");
      Log("LOG", message);
      return CONTINUE;
    } else if (type == "Shutdown") {
      std::string message;
      if (!msg->GetString("message", &message))
        return MissingField(type, "message");
      if (!msg->GetBoolean("passed", &test_passed_))
        return MissingField(type, "passed");
      Log("SHUTDOWN", message);
      return DONE;
    } else {
      return InternalError("Unknown message type: " + type);
    }
  }

  bool test_passed() const {
    return test_passed_;
  }

 private:
  bool test_passed_;

  DISALLOW_COPY_AND_ASSIGN(LoadTestMessageHandler);
};

// This class captures a stream of automation messages coming from a Javascript
// test and dispatches them to a message handler.
// TODO(ncbray) factor out and share with PPAPI tests.
class JavascriptTestObserver : public content::NotificationObserver {
 public:
  JavascriptTestObserver(
      content::RenderViewHost* render_view_host,
      TestMessageHandler* handler)
      : handler_(handler),
        running_(false) {
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::Source<content::RenderViewHost>(render_view_host));
  }

  // Pump the message loop until the message handler indicates the Javascript
  // test is done running.  Return true if the test jig functioned correctly and
  // nothing timed out.
  bool Run() {
    running_ = true;
    content::RunMessageLoop();
    running_ = false;
    return handler_->ok();
  }

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == content::NOTIFICATION_DOM_OPERATION_RESPONSE);
    content::Details<content::DomOperationNotificationDetails> dom_op_details(
        details);
    // We might receive responses for other script execution, but we only
    // care about the test finished message.
    TestMessageHandler::MessageResponse response =
        handler_->HandleMessage(dom_op_details->json);

    if (response == TestMessageHandler::DONE) {
      EndTest();
    } else {
      Continue();
    }
  }

 private:
  void Continue() {
  }

  void EndTest() {
    if (running_) {
      MessageLoopForUI::current()->Quit();
    }
  }

  TestMessageHandler* handler_;
  bool running_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptTestObserver);
};

// NaCl browser tests serve files out of the build directory because nexes and
// pexes are artifacts of the build.  To keep things tidy, all test data is kept
// in a subdirectory.  Several variants of a test may be run, for example when
// linked against newlib and when linked against glibc.  These variants are kept
// in different subdirectories.  For example, the build directory will look
// something like this on Linux:
// out/
//     Release/
//             nacl_test_data/
//                            newlib/
//                            glibc/
bool GetNaClVariantRoot(const FilePath::StringType& variant,
                        FilePath* document_root) {
  if (!ui_test_utils::GetRelativeBuildDirectory(document_root))
    return false;
  *document_root = document_root->Append(FILE_PATH_LITERAL("nacl_test_data"));
  *document_root = document_root->Append(variant);
  return true;
}

class NaClBrowserTestBase : public InProcessBrowserTest {
 public:
  NaClBrowserTestBase() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kNoFirstRun);
    command_line->AppendSwitch(switches::kEnableNaCl);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Sanity check.
    FilePath plugin_lib;
    ASSERT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
    ASSERT_TRUE(file_util::PathExists(plugin_lib)) << plugin_lib.value();

    ASSERT_TRUE(StartTestServer()) << "Cannot start test server.";
  }

  virtual FilePath::StringType Variant() = 0;

  GURL TestURL(const FilePath::StringType& test_file) {
    FilePath real_path = test_server_->document_root().Append(test_file);
    EXPECT_TRUE(file_util::PathExists(real_path)) << real_path.value();

    FilePath url_path = FilePath(FILE_PATH_LITERAL("files"));
    url_path = url_path.Append(test_file);
    return test_server_->GetURL(url_path.MaybeAsASCII());
  }

  bool RunJavascriptTest(const GURL& url, TestMessageHandler* handler) {
    JavascriptTestObserver observer(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
        handler);
    ui_test_utils::NavigateToURL(browser(), url);
    return observer.Run();
  }

  void RunLoadTest(const FilePath::StringType& test_file) {
    LoadTestMessageHandler handler;
    bool ok = RunJavascriptTest(TestURL(test_file), &handler);
    ASSERT_TRUE(ok) << handler.error_message();
    ASSERT_TRUE(handler.test_passed()) << "Test failed.";
  }

 private:
  bool StartTestServer() {
    // Launch the web server.
    FilePath document_root;
    if (!GetNaClVariantRoot(Variant(), &document_root))
      return false;
    test_server_.reset(new net::TestServer(net::TestServer::TYPE_HTTP,
                                           net::TestServer::kLocalhost,
                                           document_root));
    return test_server_->Start();
  }

  scoped_ptr<net::TestServer> test_server_;
};

class NaClBrowserTestNewlib : public NaClBrowserTestBase {
  virtual FilePath::StringType Variant() {
    return FILE_PATH_LITERAL("newlib");
  }
};

class NaClBrowserTestGLibc : public NaClBrowserTestBase {
  virtual FilePath::StringType Variant() {
    return FILE_PATH_LITERAL("glibc");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, SimpleLoadTest) {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibc, SimpleLoadTest) {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
}

}  // namespace anonymous
