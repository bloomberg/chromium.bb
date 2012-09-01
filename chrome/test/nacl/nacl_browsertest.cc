// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/javascript_test_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "net/base/net_util.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

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

  // This method provides a higher-level interface for handling JSON messages
  // from the DOM automation controler.  Instead of handling a string
  // containing a JSON-encoded object, this specialization of TestMessageHandler
  // decodes the string into a dictionary. This makes it easier to send and
  // receive structured messages.  It is assumed the dictionary will always have
  // a "type" field that indicates the nature of message.
  virtual MessageResponse HandleStructuredMessage(
      const std::string& type,
      base::DictionaryValue* msg) = 0;

 protected:
  // The structured message is missing an expected field.
  MessageResponse MissingField(
      const std::string& type,
      const std::string& field) WARN_UNUSED_RESULT {
    return InternalError(type + " message did not have field: " + field);
  }

  // Somthing went wrong while decodind the message.
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

// Disable tests under Linux ASAN.
// Linux ASAN doesn't work with NaCl.  See: http://crbug.com/104832.
// TODO(ncbray) enable after http://codereview.chromium.org/10830009/ lands.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, SimpleLoadTest) {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibc, SimpleLoadTest) {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
}

class HistogramHelper {
 public:
  HistogramHelper() {}

  // Each child process may have its own histogram data, make sure this data
  // gets accumulated into the browser process before we examine the histograms.
  void Fetch() {
    base::Closure callback = base::Bind(&HistogramHelper::FetchCallback,
                                        base::Unretained(this));

    content::FetchHistogramsAsynchronously(
        MessageLoop::current(),
        callback,
        // Give up after 60 seconds, which is longer than the 45 second timeout
        // for browser tests.  If this call times out, it means that a child
        // process is not responding which is something we should not ignore.
        base::TimeDelta::FromMilliseconds(60000));
    content::RunMessageLoop();
  }

  // We know the exact number of samples in a bucket, and that no other bucket
  // should have samples.
  void ExpectUniqueSample(const std::string& name, size_t bucket_id,
                          base::Histogram::Count expected_count) {
    base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
    ASSERT_NE(static_cast<base::Histogram*>(NULL), histogram) <<
        "Histogram \"" << name << "\" does not exist.";

    base::Histogram::SampleSet samples;
    histogram->SnapshotSample(&samples);
    CheckBucketCount(name, bucket_id, expected_count, samples);
    CheckTotalCount(name, expected_count, samples);
  }

  // We don't know the values of the samples, but we know how many there are.
  void ExpectTotalCount(const std::string& name, base::Histogram::Count count) {
    base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
    ASSERT_NE((base::Histogram*)NULL, histogram) << "Histogram \"" << name <<
        "\" does not exist.";

    base::Histogram::SampleSet samples;
    histogram->SnapshotSample(&samples);
    CheckTotalCount(name, count, samples);
  }

 private:
  void FetchCallback() {
    MessageLoopForUI::current()->Quit();
  }

  void CheckBucketCount(const std::string& name, size_t bucket_id,
                        base::Histogram::Count expected_count,
                        base::Histogram::SampleSet& samples) {
    EXPECT_EQ(expected_count, samples.counts(bucket_id)) << "Histogram \"" <<
        name << "\" does not have the right number of samples (" <<
        expected_count << ") in the expected bucket (" << bucket_id << ").";
  }

  void CheckTotalCount(const std::string& name,
                       base::Histogram::Count expected_count,
                       base::Histogram::SampleSet& samples) {
    EXPECT_EQ(expected_count, samples.TotalCount()) << "Histogram \"" << name <<
        "\" does not have the right total number of samples (" <<
        expected_count << ").";
  }
};

// TODO(ncbray) create multiple variants (newlib, glibc, pnacl) of the same test
// via macros.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, UMALoadTest) {
  // Sanity check.
  ASSERT_TRUE(base::StatisticsRecorder::IsActive());

  // Load a NaCl module to generate UMA data.
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  HistogramHelper histograms;
  histograms.Fetch();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                plugin::ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr",
                                LOAD_OK, 1);

  // Make sure we have other important histograms.
  histograms.ExpectTotalCount("NaCl.Perf.StartupTime.LoadModule", 1);
  histograms.ExpectTotalCount("NaCl.Perf.StartupTime.Total", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Nexe", 1);
}

// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

#endif  // !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

}  // namespace anonymous
