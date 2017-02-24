// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_file_writer.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/net_log/chrome_net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kChannelString[] = "SomeChannel";

// Keep this in sync with kLogRelativePath defined in net_log_file_writer.cc.
base::FilePath::CharType kLogRelativePath[] =
    FILE_PATH_LITERAL("net-export/chrome-net-export-log.json");

const char kCaptureModeDefaultString[] = "STRIP_PRIVATE_DATA";
const char kCaptureModeIncludeCookiesAndCredentialsString[] = "NORMAL";
const char kCaptureModeIncludeSocketBytesString[] = "LOG_BYTES";

const char kStateUninitializedString[] = "UNINITIALIZED";
const char kStateInitializingString[] = "INITIALIZING";
const char kStateNotLoggingString[] = "NOT_LOGGING";
const char kStateStartingLogString[] = "STARTING_LOG";
const char kStateLoggingString[] = "LOGGING";
const char kStateStoppingLogString[] = "STOPPING_LOG";
}  // namespace

namespace net_log {

// Sets |path| to |path_to_return| and always returns true. This function is
// used to override NetLogFileWriter's usual getter for the default log base
// directory.
bool SetPathToGivenAndReturnTrue(const base::FilePath& path_to_return,
                                 base::FilePath* path) {
  *path = path_to_return;
  return true;
}

// Checks the "state" string of a NetLogFileWriter state.
WARN_UNUSED_RESULT ::testing::AssertionResult VerifyState(
    std::unique_ptr<base::DictionaryValue> state,
    const std::string& expected_state_string) {
  std::string actual_state_string;
  if (!state->GetString("state", &actual_state_string)) {
    return ::testing::AssertionFailure()
           << "State is missing \"state\" string.";
  }
  if (actual_state_string != expected_state_string) {
    return ::testing::AssertionFailure()
           << "\"state\" string of state does not match expected." << std::endl
           << "    Actual: " << actual_state_string << std::endl
           << "  Expected: " << expected_state_string;
  }
  return ::testing::AssertionSuccess();
}

// Checks all fields of a NetLogFileWriter state except possibly the
// "captureMode" string; that field is only checked if
// |expected_log_capture_mode_known| is true.
WARN_UNUSED_RESULT ::testing::AssertionResult VerifyState(
    std::unique_ptr<base::DictionaryValue> state,
    const std::string& expected_state_string,
    bool expected_log_exists,
    bool expected_log_capture_mode_known,
    const std::string& expected_log_capture_mode_string) {
  base::DictionaryValue expected_state;
  expected_state.SetString("state", expected_state_string);
  expected_state.SetBoolean("logExists", expected_log_exists);
  expected_state.SetBoolean("logCaptureModeKnown",
                            expected_log_capture_mode_known);
  if (expected_log_capture_mode_known) {
    expected_state.SetString("captureMode", expected_log_capture_mode_string);
  } else {
    state->Remove("captureMode", nullptr);
  }

  // Remove "file" field which is only added in debug mode.
  state->Remove("file", nullptr);

  std::string expected_state_json_string;
  JSONStringValueSerializer expected_state_serializer(
      &expected_state_json_string);
  expected_state_serializer.Serialize(expected_state);

  std::string actual_state_json_string;
  JSONStringValueSerializer actual_state_serializer(&actual_state_json_string);
  actual_state_serializer.Serialize(*state);

  if (actual_state_json_string != expected_state_json_string) {
    return ::testing::AssertionFailure()
           << "State (possibly excluding \"captureMode\") does not match "
              "expected."
           << std::endl
           << "    Actual: " << actual_state_json_string << std::endl
           << "  Expected: " << expected_state_json_string;
  }
  return ::testing::AssertionSuccess();
}

WARN_UNUSED_RESULT ::testing::AssertionResult ReadCompleteLogFile(
    const base::FilePath& log_path,
    std::unique_ptr<base::DictionaryValue>* root) {
  DCHECK(!log_path.empty());

  if (!base::PathExists(log_path)) {
    return ::testing::AssertionFailure() << log_path.value()
                                         << " does not exist.";
  }
  // Parse log file contents into a dictionary
  std::string log_string;
  if (!base::ReadFileToString(log_path, &log_string)) {
    return ::testing::AssertionFailure() << log_path.value()
                                         << " could not be read.";
  }
  *root = base::DictionaryValue::From(base::JSONReader::Read(log_string));
  if (!*root) {
    return ::testing::AssertionFailure()
           << "Contents of " << log_path.value()
           << " do not form valid JSON dictionary.";
  }
  // Make sure the "constants" section exists
  base::DictionaryValue* constants;
  if (!(*root)->GetDictionary("constants", &constants)) {
    root->reset();
    return ::testing::AssertionFailure() << log_path.value()
                                         << " is missing constants.";
  }
  // Make sure the "events" section exists
  base::ListValue* events;
  if (!(*root)->GetList("events", &events)) {
    root->reset();
    return ::testing::AssertionFailure() << log_path.value()
                                         << " is missing events list.";
  }
  return ::testing::AssertionSuccess();
}

void SetUpTestContextGetterWithQuicTimeoutInfo(
    net::NetLog* net_log,
    int quic_idle_connection_timeout_seconds,
    scoped_refptr<net::TestURLRequestContextGetter>* context_getter) {
  std::unique_ptr<net::TestURLRequestContext> context =
      base::MakeUnique<net::TestURLRequestContext>(true);
  context->set_net_log(net_log);

  std::unique_ptr<net::HttpNetworkSession::Params> params(
      new net::HttpNetworkSession::Params);
  params->quic_idle_connection_timeout_seconds =
      quic_idle_connection_timeout_seconds;

  context->set_http_network_session_params(std::move(params));
  context->Init();

  *context_getter = new net::TestURLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(), std::move(context));
}

void SetUpTestContextGetterWithRequest(
    net::NetLog* net_log,
    const GURL& url,
    net::URLRequest::Delegate* delegate,
    scoped_refptr<net::TestURLRequestContextGetter>* context_getter,
    std::unique_ptr<net::URLRequest>* request) {
  auto context = base::MakeUnique<net::TestURLRequestContext>(true);
  context->set_net_log(net_log);
  context->Init();

  *request = context->CreateRequest(url, net::IDLE, delegate);
  (*request)->Start();

  *context_getter = new net::TestURLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(), std::move(context));
}

// An implementation of NetLogFileWriter::StateObserver that allows waiting
// until it's notified of a new state.
class TestStateObserver : public NetLogFileWriter::StateObserver {
 public:
  // NetLogFileWriter::StateObserver implementation
  void OnNewState(const base::DictionaryValue& state) override {
    test_closure_.closure().Run();
    result_state_ = state.CreateDeepCopy();
  }

  std::unique_ptr<base::DictionaryValue> WaitForNewState() {
    test_closure_.WaitForResult();
    DCHECK(result_state_);
    return std::move(result_state_);
  }

 private:
  net::TestClosure test_closure_;
  std::unique_ptr<base::DictionaryValue> result_state_;
};

// A class that wraps around TestClosure. Provides the ability to wait on a
// file path callback and retrieve the result.
class TestFilePathCallback {
 public:
  TestFilePathCallback()
      : callback_(base::Bind(&TestFilePathCallback::SetResultThenNotify,
                             base::Unretained(this))) {}

  const base::Callback<void(const base::FilePath&)>& callback() const {
    return callback_;
  }

  const base::FilePath& WaitForResult() {
    test_closure_.WaitForResult();
    return result_;
  }

 private:
  void SetResultThenNotify(const base::FilePath& result) {
    result_ = result;
    test_closure_.closure().Run();
  }

  net::TestClosure test_closure_;
  base::FilePath result_;
  base::Callback<void(const base::FilePath&)> callback_;
};

class NetLogFileWriterTest : public ::testing::Test {
 public:
  using URLRequestContextGetterList =
      std::vector<scoped_refptr<net::URLRequestContextGetter>>;

  NetLogFileWriterTest()
      : net_log_(base::FilePath(),
                 net::NetLogCaptureMode::Default(),
                 base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
                 kChannelString),
        net_log_file_writer_(
            &net_log_,
            base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
            kChannelString),
        file_thread_("NetLogFileWriter file thread"),
        net_thread_("NetLogFileWriter net thread") {}

  // ::testing::Test implementation
  void SetUp() override {
    ASSERT_TRUE(log_temp_dir_.CreateUniqueTempDir());

    // Override |net_log_file_writer_|'s default-log-base-directory-getter to
    // a getter that returns the temp dir created for the test.
    net_log_file_writer_.SetDefaultLogBaseDirectoryGetterForTest(
        base::Bind(&SetPathToGivenAndReturnTrue, log_temp_dir_.GetPath()));

    default_log_path_ = log_temp_dir_.GetPath().Append(kLogRelativePath);

    ASSERT_TRUE(file_thread_.Start());
    ASSERT_TRUE(net_thread_.Start());

    net_log_file_writer_.AddObserver(&test_state_observer_);

    ASSERT_TRUE(VerifyState(net_log_file_writer_.GetState(),
                            kStateUninitializedString, false, false, ""));
  }

  // ::testing::Test implementation
  void TearDown() override {
    net_log_file_writer_.RemoveObserver(&test_state_observer_);
    ASSERT_TRUE(log_temp_dir_.Delete());
  }

  base::FilePath FileWriterGetFilePathToCompletedLog() {
    TestFilePathCallback test_callback;
    net_log_file_writer_.GetFilePathToCompletedLog(test_callback.callback());
    return test_callback.WaitForResult();
  }

  WARN_UNUSED_RESULT ::testing::AssertionResult InitializeThenVerifyNewState(
      bool expected_initialize_success,
      bool expected_log_exists) {
    net_log_file_writer_.Initialize(file_thread_.task_runner(),
                                    net_thread_.task_runner());
    std::unique_ptr<base::DictionaryValue> state =
        test_state_observer_.WaitForNewState();
    ::testing::AssertionResult result =
        VerifyState(std::move(state), kStateInitializingString);
    if (!result) {
      return ::testing::AssertionFailure()
             << "First state after Initialize() does not match expected:"
             << std::endl
             << result.message();
    }

    state = test_state_observer_.WaitForNewState();
    result = VerifyState(std::move(state), expected_initialize_success
                                               ? kStateNotLoggingString
                                               : kStateUninitializedString,
                         expected_log_exists, false, "");
    if (!result) {
      return ::testing::AssertionFailure()
             << "Second state after Initialize() does not match expected:"
             << std::endl
             << result.message();
    }

    return ::testing::AssertionSuccess();
  }

  // If |custom_log_path| is empty path, |net_log_file_writer_| will use its
  // default log path, which is cached in |default_log_path_|.
  WARN_UNUSED_RESULT::testing::AssertionResult StartThenVerifyNewState(
      const base::FilePath& custom_log_path,
      net::NetLogCaptureMode capture_mode,
      const std::string& expected_capture_mode_string,
      const URLRequestContextGetterList& context_getters) {
    net_log_file_writer_.StartNetLog(custom_log_path, capture_mode,
                                     context_getters);
    std::unique_ptr<base::DictionaryValue> state =
        test_state_observer_.WaitForNewState();
    ::testing::AssertionResult result =
        VerifyState(std::move(state), kStateStartingLogString);
    if (!result) {
      return ::testing::AssertionFailure()
             << "First state after StartNetLog() does not match expected:"
             << std::endl
             << result.message();
    }

    state = test_state_observer_.WaitForNewState();
    result = VerifyState(std::move(state), kStateLoggingString, true, true,
                         expected_capture_mode_string);
    if (!result) {
      return ::testing::AssertionFailure()
             << "Second state after StartNetLog() does not match expected:"
             << std::endl
             << result.message();
    }

    // Make sure GetFilePath() returns empty path when logging.
    base::FilePath actual_log_path = FileWriterGetFilePathToCompletedLog();
    if (!actual_log_path.empty()) {
      return ::testing::AssertionFailure()
             << "GetFilePath() should return empty path after logging starts."
             << " Actual: " << ::testing::PrintToString(actual_log_path);
    }

    return ::testing::AssertionSuccess();
  }

  // If |custom_log_path| is empty path, it's assumed the log file with be at
  // |default_log_path_|.
  WARN_UNUSED_RESULT ::testing::AssertionResult StopThenVerifyNewStateAndFile(
      const base::FilePath& custom_log_path,
      std::unique_ptr<base::DictionaryValue> polled_data,
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      const std::string& expected_capture_mode_string) {
    net_log_file_writer_.StopNetLog(std::move(polled_data), context_getter);
    std::unique_ptr<base::DictionaryValue> state =
        test_state_observer_.WaitForNewState();
    ::testing::AssertionResult result =
        VerifyState(std::move(state), kStateStoppingLogString);
    if (!result) {
      return ::testing::AssertionFailure()
             << "First state after StopNetLog() does not match expected:"
             << std::endl
             << result.message();
    }

    state = test_state_observer_.WaitForNewState();
    result = VerifyState(std::move(state), kStateNotLoggingString, true, true,
                         expected_capture_mode_string);
    if (!result) {
      return ::testing::AssertionFailure()
             << "Second state after StopNetLog() does not match expected:"
             << std::endl
             << result.message();
    }

    // Make sure GetFilePath() returns the expected path.
    const base::FilePath& expected_log_path =
        custom_log_path.empty() ? default_log_path_ : custom_log_path;
    base::FilePath actual_log_path = FileWriterGetFilePathToCompletedLog();
    if (actual_log_path != expected_log_path) {
      return ::testing::AssertionFailure()
             << "GetFilePath() returned incorrect path after logging stopped."
             << std::endl
             << "    Actual: " << ::testing::PrintToString(actual_log_path)
             << std::endl
             << "  Expected: " << ::testing::PrintToString(expected_log_path);
    }

    // Make sure the generated log file is valid.
    std::unique_ptr<base::DictionaryValue> root;
    result = ReadCompleteLogFile(expected_log_path, &root);
    if (!result) {
      return ::testing::AssertionFailure()
             << "Log file after logging stopped is not valid:" << std::endl
             << result.message();
    }

    return ::testing::AssertionSuccess();
  }

 protected:
  ChromeNetLog net_log_;

  // |net_log_file_writer_| is initialized after |net_log_| so that it can stop
  // obvserving on destruction.
  NetLogFileWriter net_log_file_writer_;

  base::ScopedTempDir log_temp_dir_;

  // The default log path that |net_log_file_writer_| will use is cached here.
  base::FilePath default_log_path_;

  base::Thread file_thread_;
  base::Thread net_thread_;

  TestStateObserver test_state_observer_;

 private:
  // Allows tasks to be posted to the main thread.
  base::MessageLoop message_loop_;
};

TEST_F(NetLogFileWriterTest, InitFail) {
  // Override net_log_file_writer_'s default log base directory getter to always
  // fail.
  net_log_file_writer_.SetDefaultLogBaseDirectoryGetterForTest(
      base::Bind([](base::FilePath* path) -> bool { return false; }));

  // Initialization should fail due to the override.
  ASSERT_TRUE(InitializeThenVerifyNewState(false, false));

  // NetLogFileWriter::GetFilePath() should return empty path if uninitialized.
  EXPECT_TRUE(FileWriterGetFilePathToCompletedLog().empty());
}

TEST_F(NetLogFileWriterTest, InitWithoutExistingLog) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  // NetLogFileWriter::GetFilePathToCompletedLog() should return empty path when
  // no log file exists.
  EXPECT_TRUE(FileWriterGetFilePathToCompletedLog().empty());
}

TEST_F(NetLogFileWriterTest, InitWithExistingLog) {
  // Create and close an empty log file to simulate existence of a previous log
  // file.
  ASSERT_TRUE(
      base::CreateDirectoryAndGetError(default_log_path_.DirName(), nullptr));
  base::ScopedFILE empty_file(base::OpenFile(default_log_path_, "w"));
  ASSERT_TRUE(empty_file.get());
  empty_file.reset();

  ASSERT_TRUE(InitializeThenVerifyNewState(true, true));

  EXPECT_EQ(default_log_path_, FileWriterGetFilePathToCompletedLog());
}

TEST_F(NetLogFileWriterTest, StartAndStopWithAllCaptureModes) {
  const net::NetLogCaptureMode capture_modes[3] = {
      net::NetLogCaptureMode::Default(),
      net::NetLogCaptureMode::IncludeCookiesAndCredentials(),
      net::NetLogCaptureMode::IncludeSocketBytes()};

  const std::string capture_mode_strings[3] = {
      kCaptureModeDefaultString, kCaptureModeIncludeCookiesAndCredentialsString,
      kCaptureModeIncludeSocketBytesString};

  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  // For each capture mode, start and stop |net_log_file_writer_| in that mode.
  for (int i = 0; i < 3; ++i) {
    // StartNetLog(), should result in state change.
    ASSERT_TRUE(StartThenVerifyNewState(base::FilePath(), capture_modes[i],
                                        capture_mode_strings[i],
                                        URLRequestContextGetterList()));

    // Calling StartNetLog() again should be a no-op. Try doing StartNetLog()
    // with various capture modes; they should all be ignored and result in no
    // state change.
    net_log_file_writer_.StartNetLog(base::FilePath(), capture_modes[i],
                                     URLRequestContextGetterList());
    net_log_file_writer_.StartNetLog(base::FilePath(),
                                     capture_modes[(i + 1) % 3],
                                     URLRequestContextGetterList());
    net_log_file_writer_.StartNetLog(base::FilePath(),
                                     capture_modes[(i + 2) % 3],
                                     URLRequestContextGetterList());

    // StopNetLog(), should result in state change. The capture mode should
    // match that of the first StartNetLog() call (called by
    // StartThenVerifyNewState()).
    ASSERT_TRUE(StopThenVerifyNewStateAndFile(
        base::FilePath(), nullptr, nullptr, capture_mode_strings[i]));

    // Stopping a second time should be a no-op.
    net_log_file_writer_.StopNetLog(nullptr, nullptr);
  }

  // Start and stop one more time just to make sure the last StopNetLog() call
  // was properly ignored and left |net_log_file_writer_| in a valid state.
  ASSERT_TRUE(StartThenVerifyNewState(base::FilePath(), capture_modes[0],
                                      capture_mode_strings[0],
                                      URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            capture_mode_strings[0]));
}

// Verify the file sizes after two consecutive starts/stops are the same (even
// if some junk data is added in between).
TEST_F(NetLogFileWriterTest, StartClearsFile) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            kCaptureModeDefaultString));

  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &stop_file_size));

  // Add some junk at the end of the file.
  std::string junk_data("Hello");
  EXPECT_TRUE(base::AppendToFile(default_log_path_, junk_data.c_str(),
                                 junk_data.size()));

  int64_t junk_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &junk_file_size));
  EXPECT_GT(junk_file_size, stop_file_size);

  // Start and stop again and make sure the file is back to the size it was
  // before adding the junk data.
  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            kCaptureModeDefaultString));

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &new_stop_file_size));

  EXPECT_EQ(stop_file_size, new_stop_file_size);
}

// Adds an event to the log file, then checks that the file is larger than
// the file created without that event.
TEST_F(NetLogFileWriterTest, AddEvent) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            kCaptureModeDefaultString));

  // Get file size without the event.
  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &stop_file_size));

  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  net_log_.AddGlobalEntry(net::NetLogEventType::CANCELLED);

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            kCaptureModeDefaultString));

  // Get file size after adding the event and make sure it's larger than before.
  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &new_stop_file_size));
  EXPECT_GE(new_stop_file_size, stop_file_size);
}

// Using a custom path to make sure logging can still occur when the path has
// changed.
TEST_F(NetLogFileWriterTest, AddEventCustomPath) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  base::FilePath::CharType kCustomRelativePath[] =
      FILE_PATH_LITERAL("custom/custom/chrome-net-export-log.json");
  base::FilePath custom_log_path =
      log_temp_dir_.GetPath().Append(kCustomRelativePath);
  EXPECT_TRUE(
      base::CreateDirectoryAndGetError(custom_log_path.DirName(), nullptr));

  ASSERT_TRUE(StartThenVerifyNewState(
      custom_log_path, net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(custom_log_path, nullptr, nullptr,
                                            kCaptureModeDefaultString));

  // Get file size without the event.
  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(custom_log_path, &stop_file_size));

  ASSERT_TRUE(StartThenVerifyNewState(
      custom_log_path, net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  net_log_.AddGlobalEntry(net::NetLogEventType::CANCELLED);

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(custom_log_path, nullptr, nullptr,
                                            kCaptureModeDefaultString));

  // Get file size after adding the event and make sure it's larger than before.
  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(custom_log_path, &new_stop_file_size));
  EXPECT_GE(new_stop_file_size, stop_file_size);
}

TEST_F(NetLogFileWriterTest, StopWithPolledDataAndContextGetter) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  // Create dummy polled data
  const char kDummyPolledDataPath[] = "dummy_path";
  const char kDummyPolledDataString[] = "dummy_info";
  std::unique_ptr<base::DictionaryValue> dummy_polled_data =
      base::MakeUnique<base::DictionaryValue>();
  dummy_polled_data->SetString(kDummyPolledDataPath, kDummyPolledDataString);

  // Create test context getter on |net_thread_| and wait for it to finish.
  scoped_refptr<net::TestURLRequestContextGetter> context_getter;
  const int kDummyQuicParam = 75;
  net::TestClosure init_done;
  net_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&SetUpTestContextGetterWithQuicTimeoutInfo,
                            &net_log_, kDummyQuicParam, &context_getter),
      init_done.closure());
  init_done.WaitForResult();

  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, URLRequestContextGetterList()));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(
      base::FilePath(), std::move(dummy_polled_data), context_getter,
      kCaptureModeDefaultString));

  // Read polledData from log file.
  std::unique_ptr<base::DictionaryValue> root;
  ASSERT_TRUE(ReadCompleteLogFile(default_log_path_, &root));
  base::DictionaryValue* polled_data;
  ASSERT_TRUE(root->GetDictionary("polledData", &polled_data));

  // Check that it contains the field from the polled data that was passed in.
  std::string dummy_string;
  ASSERT_TRUE(polled_data->GetString(kDummyPolledDataPath, &dummy_string));
  EXPECT_EQ(kDummyPolledDataString, dummy_string);

  // Check that it also contains the field from the URLRequestContext that was
  // passed in.
  base::DictionaryValue* quic_info;
  ASSERT_TRUE(polled_data->GetDictionary("quicInfo", &quic_info));
  base::Value* timeout_value = nullptr;
  int timeout;
  ASSERT_TRUE(
      quic_info->Get("idle_connection_timeout_seconds", &timeout_value));
  ASSERT_TRUE(timeout_value->GetAsInteger(&timeout));
  EXPECT_EQ(kDummyQuicParam, timeout);
}

TEST_F(NetLogFileWriterTest, StartWithContextGetters) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  // Create test context getter and request on |net_thread_| and wait for it to
  // finish.
  const std::string kDummyUrl = "blah:blah";
  scoped_refptr<net::TestURLRequestContextGetter> context_getter;
  std::unique_ptr<net::URLRequest> request;
  net::TestDelegate delegate;
  delegate.set_quit_on_complete(false);

  net::TestClosure init_done;
  net_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&SetUpTestContextGetterWithRequest, &net_log_, GURL(kDummyUrl),
                 &delegate, &context_getter, &request),
      init_done.closure());
  init_done.WaitForResult();

  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      kCaptureModeDefaultString, {context_getter}));

  ASSERT_TRUE(StopThenVerifyNewStateAndFile(base::FilePath(), nullptr, nullptr,
                                            kCaptureModeDefaultString));

  // Read events from log file.
  std::unique_ptr<base::DictionaryValue> root;
  ASSERT_TRUE(ReadCompleteLogFile(default_log_path_, &root));
  base::ListValue* events;
  ASSERT_TRUE(root->GetList("events", &events));

  // Check there is at least one event as a result of the ongoing request.
  EXPECT_GE(events->GetSize(), 1u);

  // Check the URL in the params of the first event.
  base::DictionaryValue* event;
  EXPECT_TRUE(events->GetDictionary(0, &event));
  base::DictionaryValue* event_params;
  EXPECT_TRUE(event->GetDictionary("params", &event_params));
  std::string event_url;
  EXPECT_TRUE(event_params->GetString("url", &event_url));
  EXPECT_EQ(kDummyUrl, event_url);

  net_thread_.task_runner()->DeleteSoon(FROM_HERE, request.release());
}

TEST_F(NetLogFileWriterTest, ReceiveStartWhileInitializing) {
  // Trigger initialization of |net_log_file_writer_|.
  net_log_file_writer_.Initialize(file_thread_.task_runner(),
                                  net_thread_.task_runner());

  // Before running the main message loop, tell |net_log_file_writer_| to start
  // logging. Not running the main message loop prevents the initialization
  // process from completing, so this ensures that StartNetLog() is received
  // before |net_log_file_writer_| finishes initialization, which means this
  // should be a no-op.
  net_log_file_writer_.StartNetLog(base::FilePath(),
                                   net::NetLogCaptureMode::Default(),
                                   URLRequestContextGetterList());

  // Now run the main message loop. Make sure StartNetLog() was ignored by
  // checking that the next two states are "initializing" followed by
  // "not-logging".
  std::unique_ptr<base::DictionaryValue> state =
      test_state_observer_.WaitForNewState();
  ASSERT_TRUE(VerifyState(std::move(state), kStateInitializingString));
  state = test_state_observer_.WaitForNewState();
  ASSERT_TRUE(
      VerifyState(std::move(state), kStateNotLoggingString, false, false, ""));
}

TEST_F(NetLogFileWriterTest, ReceiveStartWhileStoppingLog) {
  ASSERT_TRUE(InitializeThenVerifyNewState(true, false));

  // Call StartNetLog() on |net_log_file_writer_| and wait for the state change.
  ASSERT_TRUE(StartThenVerifyNewState(
      base::FilePath(), net::NetLogCaptureMode::IncludeSocketBytes(),
      kCaptureModeIncludeSocketBytesString, URLRequestContextGetterList()));

  // Tell |net_log_file_writer_| to stop logging.
  net_log_file_writer_.StopNetLog(nullptr, nullptr);

  // Before running the main message loop, tell |net_log_file_writer_| to start
  // logging. Not running the main message loop prevents the stopping process
  // from completing, so this ensures StartNetLog() is received before
  // |net_log_file_writer_| finishes stopping, which means this should be a
  // no-op.
  net_log_file_writer_.StartNetLog(base::FilePath(),
                                   net::NetLogCaptureMode::Default(),
                                   URLRequestContextGetterList());

  // Now run the main message loop. Make sure the last StartNetLog() was
  // ignored by checking that the next two states are "stopping-log" followed by
  // "not-logging". Also make sure the capture mode matches that of the first
  // StartNetLog() call (called by StartThenVerifyState()).
  std::unique_ptr<base::DictionaryValue> state =
      test_state_observer_.WaitForNewState();
  ASSERT_TRUE(VerifyState(std::move(state), kStateStoppingLogString));
  state = test_state_observer_.WaitForNewState();
  ASSERT_TRUE(VerifyState(std::move(state), kStateNotLoggingString, true, true,
                          kCaptureModeIncludeSocketBytesString));
}

}  // namespace net_log
