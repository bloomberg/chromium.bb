// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_file_writer.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
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
#include "net/log/write_to_file_net_log_observer.h"
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
const char kStateNotLoggingString[] = "NOT_LOGGING";
const char kStateLoggingString[] = "LOGGING";

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

// Checks the fields of the state returned by NetLogFileWriter's state callback.
// |expected_log_capture_mode_string| is only checked if
// |expected_log_capture_mode_known| is true.
void VerifyState(const base::DictionaryValue& state,
                 const std::string& expected_state_string,
                 bool expected_log_exists,
                 bool expected_log_capture_mode_known,
                 const std::string& expected_log_capture_mode_string) {
  std::string state_string;
  ASSERT_TRUE(state.GetString("state", &state_string));
  EXPECT_EQ(state_string, expected_state_string);

  bool log_exists;
  ASSERT_TRUE(state.GetBoolean("logExists", &log_exists));
  EXPECT_EQ(log_exists, expected_log_exists);

  bool log_capture_mode_known;
  ASSERT_TRUE(state.GetBoolean("logCaptureModeKnown", &log_capture_mode_known));
  EXPECT_EQ(log_capture_mode_known, expected_log_capture_mode_known);

  if (expected_log_capture_mode_known) {
    std::string log_capture_mode_string;
    ASSERT_TRUE(state.GetString("captureMode", &log_capture_mode_string));
    EXPECT_EQ(log_capture_mode_string, expected_log_capture_mode_string);
  }
}

::testing::AssertionResult ReadCompleteLogFile(
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
                                         << " does not contain constants.";
  }
  // Make sure the "events" section exists
  base::ListValue* events;
  if (!(*root)->GetList("events", &events)) {
    root->reset();
    return ::testing::AssertionFailure() << log_path.value()
                                         << " does not contain events list.";
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

// A class that wraps around TestClosure. Provides the ability to wait on a
// state callback and retrieve the result.
class TestStateCallback {
 public:
  TestStateCallback()
      : callback_(base::Bind(&TestStateCallback::SetResultThenNotify,
                             base::Unretained(this))) {}

  const base::Callback<void(std::unique_ptr<base::DictionaryValue>)>& callback()
      const {
    return callback_;
  }

  std::unique_ptr<base::DictionaryValue> WaitForResult() {
    test_closure_.WaitForResult();
    return std::move(result_);
  }

 private:
  void SetResultThenNotify(std::unique_ptr<base::DictionaryValue> result) {
    result_ = std::move(result);
    test_closure_.closure().Run();
  }

  net::TestClosure test_closure_;
  std::unique_ptr<base::DictionaryValue> result_;
  base::Callback<void(std::unique_ptr<base::DictionaryValue>)> callback_;
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

    net_log_file_writer_.SetTaskRunners(file_thread_.task_runner(),
                                        net_thread_.task_runner());
  }
  void TearDown() override { ASSERT_TRUE(log_temp_dir_.Delete()); }

  std::unique_ptr<base::DictionaryValue> FileWriterGetState() {
    TestStateCallback test_callback;
    net_log_file_writer_.GetState(test_callback.callback());
    return test_callback.WaitForResult();
  }

  base::FilePath FileWriterGetFilePathToCompletedLog() {
    TestFilePathCallback test_callback;
    net_log_file_writer_.GetFilePathToCompletedLog(test_callback.callback());
    return test_callback.WaitForResult();
  }

  // If |custom_log_path| is empty path, |net_log_file_writer_| will use its
  // default log path.
  void StartThenVerifyState(const base::FilePath& custom_log_path,
                            net::NetLogCaptureMode capture_mode,
                            const std::string& expected_capture_mode_string) {
    TestStateCallback test_callback;
    net_log_file_writer_.StartNetLog(custom_log_path, capture_mode,
                                     test_callback.callback());
    std::unique_ptr<base::DictionaryValue> state =
        test_callback.WaitForResult();
    VerifyState(*state, kStateLoggingString, true, true,
                expected_capture_mode_string);

    // Make sure NetLogFileWriter::GetFilePath() returns empty path when
    // logging.
    EXPECT_TRUE(FileWriterGetFilePathToCompletedLog().empty());
  }

  // If |custom_log_path| is empty path, it's assumed the log file with be at
  // |default_path_|.
  void StopThenVerifyStateAndFile(
      const base::FilePath& custom_log_path,
      std::unique_ptr<base::DictionaryValue> polled_data,
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      const std::string& expected_capture_mode_string) {
    TestStateCallback test_callback;
    net_log_file_writer_.StopNetLog(std::move(polled_data), context_getter,
                                    test_callback.callback());
    std::unique_ptr<base::DictionaryValue> state =
        test_callback.WaitForResult();
    VerifyState(*state, kStateNotLoggingString, true, true,
                expected_capture_mode_string);

    const base::FilePath& log_path =
        custom_log_path.empty() ? default_log_path_ : custom_log_path;
    EXPECT_EQ(FileWriterGetFilePathToCompletedLog(), log_path);

    std::unique_ptr<base::DictionaryValue> root;
    ASSERT_TRUE(ReadCompleteLogFile(log_path, &root));
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

 private:
  // Allows tasks to be posted to the main thread.
  base::MessageLoop message_loop_;
};

TEST_F(NetLogFileWriterTest, InitFail) {
  // Override net_log_file_writer_'s default log base directory getter to always
  // fail.
  net_log_file_writer_.SetDefaultLogBaseDirectoryGetterForTest(
      base::Bind([](base::FilePath* path) -> bool { return false; }));

  // GetState() will cause |net_log_file_writer_| to initialize. In this case,
  // initialization will fail since |net_log_file_writer_| will fail to
  // retrieve the default log base directory.
  VerifyState(*FileWriterGetState(), kStateUninitializedString, false, false,
              "");

  // NetLogFileWriter::GetFilePath() should return empty path if uninitialized.
  EXPECT_TRUE(FileWriterGetFilePathToCompletedLog().empty());
}

TEST_F(NetLogFileWriterTest, InitWithoutExistingLog) {
  // GetState() will cause |net_log_file_writer_| to initialize.
  VerifyState(*FileWriterGetState(), kStateNotLoggingString, false, false, "");

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

  // GetState() will cause |net_log_file_writer_| to initialize.
  VerifyState(*FileWriterGetState(), kStateNotLoggingString, true, false, "");

  EXPECT_EQ(FileWriterGetFilePathToCompletedLog(), default_log_path_);
}

TEST_F(NetLogFileWriterTest, StartAndStopWithAllCaptureModes) {
  const net::NetLogCaptureMode capture_modes[3] = {
      net::NetLogCaptureMode::Default(),
      net::NetLogCaptureMode::IncludeCookiesAndCredentials(),
      net::NetLogCaptureMode::IncludeSocketBytes()};

  const std::string capture_mode_strings[3] = {
      kCaptureModeDefaultString, kCaptureModeIncludeCookiesAndCredentialsString,
      kCaptureModeIncludeSocketBytesString};

  // For each capture mode, start and stop |net_log_file_writer_| in that mode.
  for (int i = 0; i < 3; ++i) {
    StartThenVerifyState(base::FilePath(), capture_modes[i],
                         capture_mode_strings[i]);

    // Starting a second time should be a no-op.
    StartThenVerifyState(base::FilePath(), capture_modes[i],
                         capture_mode_strings[i]);

    // Starting with other capture modes should also be no-ops. This should also
    // not affect the capture mode reported by |net_log_file_writer_|.
    StartThenVerifyState(base::FilePath(), capture_modes[(i + 1) % 3],
                         capture_mode_strings[i]);

    StartThenVerifyState(base::FilePath(), capture_modes[(i + 2) % 3],
                         capture_mode_strings[i]);

    StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                               capture_mode_strings[i]);

    // Stopping a second time should be a no-op.
    StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                               capture_mode_strings[i]);
  }
}

// Verify the file sizes after two consecutive starts/stops are the same (even
// if some junk data is added in between).
TEST_F(NetLogFileWriterTest, StartClearsFile) {
  StartThenVerifyState(base::FilePath(), net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                             kCaptureModeDefaultString);

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
  StartThenVerifyState(base::FilePath(), net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                             kCaptureModeDefaultString);

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &new_stop_file_size));

  EXPECT_EQ(new_stop_file_size, stop_file_size);
}

// Adds an event to the log file, then checks that the file is larger than
// the file created without that event.
TEST_F(NetLogFileWriterTest, AddEvent) {
  StartThenVerifyState(base::FilePath(), net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                             kCaptureModeDefaultString);

  // Get file size without the event.
  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &stop_file_size));

  StartThenVerifyState(base::FilePath(), net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  net_log_.AddGlobalEntry(net::NetLogEventType::CANCELLED);

  StopThenVerifyStateAndFile(base::FilePath(), nullptr, nullptr,
                             kCaptureModeDefaultString);

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(default_log_path_, &new_stop_file_size));

  EXPECT_GE(new_stop_file_size, stop_file_size);
}

// Using a custom path to make sure logging can still occur when
// the path has changed.
TEST_F(NetLogFileWriterTest, AddEventCustomPath) {
  base::FilePath::CharType kCustomRelativePath[] =
      FILE_PATH_LITERAL("custom/custom/chrome-net-export-log.json");
  base::FilePath custom_log_path =
      log_temp_dir_.GetPath().Append(kCustomRelativePath);
  EXPECT_TRUE(
      base::CreateDirectoryAndGetError(custom_log_path.DirName(), nullptr));

  StartThenVerifyState(custom_log_path, net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  StopThenVerifyStateAndFile(custom_log_path, nullptr, nullptr,
                             kCaptureModeDefaultString);

  // Get file size without the event.
  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(custom_log_path, &stop_file_size));

  StartThenVerifyState(custom_log_path, net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  net_log_.AddGlobalEntry(net::NetLogEventType::CANCELLED);

  StopThenVerifyStateAndFile(custom_log_path, nullptr, nullptr,
                             kCaptureModeDefaultString);

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(custom_log_path, &new_stop_file_size));
  EXPECT_GE(new_stop_file_size, stop_file_size);
}

TEST_F(NetLogFileWriterTest, StopWithPolledDataAndContextGetter) {
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

  StartThenVerifyState(base::FilePath(), net::NetLogCaptureMode::Default(),
                       kCaptureModeDefaultString);

  StopThenVerifyStateAndFile(base::FilePath(), std::move(dummy_polled_data),
                             context_getter, kCaptureModeDefaultString);

  // Read polledData from log file.
  std::unique_ptr<base::DictionaryValue> root;
  ASSERT_TRUE(ReadCompleteLogFile(default_log_path_, &root));
  base::DictionaryValue* polled_data;
  ASSERT_TRUE(root->GetDictionary("polledData", &polled_data));

  // Check that it contains the field from the polled data that was passed in.
  std::string dummy_string;
  ASSERT_TRUE(polled_data->GetString(kDummyPolledDataPath, &dummy_string));
  EXPECT_EQ(dummy_string, kDummyPolledDataString);

  // Check that it also contains the field from the URLRequestContext that was
  // passed in.
  base::DictionaryValue* quic_info;
  ASSERT_TRUE(polled_data->GetDictionary("quicInfo", &quic_info));
  base::Value* timeout_value = nullptr;
  int timeout;
  ASSERT_TRUE(
      quic_info->Get("idle_connection_timeout_seconds", &timeout_value));
  ASSERT_TRUE(timeout_value->GetAsInteger(&timeout));
  EXPECT_EQ(timeout, kDummyQuicParam);
}

TEST_F(NetLogFileWriterTest, ReceiveStartWhileInitializing) {
  // Trigger initialization of |net_log_file_writer_|.
  TestStateCallback init_callback;
  net_log_file_writer_.GetState(init_callback.callback());

  // Before running the main message loop, tell |net_log_file_writer_| to start
  // logging. Not running the main message loop prevents the initialization
  // process from completing, so this ensures that StartNetLog() is received
  // before |net_log_file_writer_| finishes initialization, which means this
  // should be a no-op.
  TestStateCallback start_during_init_callback;
  net_log_file_writer_.StartNetLog(base::FilePath(),
                                   net::NetLogCaptureMode::Default(),
                                   start_during_init_callback.callback());

  // Now run the main message loop.
  std::unique_ptr<base::DictionaryValue> init_callback_state =
      init_callback.WaitForResult();

  // The state returned by the GetState() call should be the state after
  // initialization, which should indicate not-logging.
  VerifyState(*init_callback_state, kStateNotLoggingString, false, false, "");

  // The state returned by the ignored StartNetLog() call should also be the
  // state after the ongoing initialization finishes, so it should be identical
  // to the state returned by GetState().
  std::unique_ptr<base::DictionaryValue> start_during_init_callback_state =
      start_during_init_callback.WaitForResult();
  VerifyState(*start_during_init_callback_state, kStateNotLoggingString, false,
              false, "");

  // Run an additional GetState() just to make sure |net_log_file_writer_| is
  // not logging.
  VerifyState(*FileWriterGetState(), kStateNotLoggingString, false, false, "");
}

TEST_F(NetLogFileWriterTest, ReceiveStartWhileStoppingLog) {
  // Call StartNetLog() on |net_log_file_writer_| and wait for it to run its
  // state callback.
  StartThenVerifyState(base::FilePath(),
                       net::NetLogCaptureMode::IncludeSocketBytes(),
                       kCaptureModeIncludeSocketBytesString);

  // Tell |net_log_file_writer_| to stop logging.
  TestStateCallback stop_callback;
  net_log_file_writer_.StopNetLog(nullptr, nullptr, stop_callback.callback());

  // Before running the main message loop, tell |net_log_file_writer_| to start
  // logging. Not running the main message loop prevents the stopping process
  // from completing, so this ensures StartNetLog() is received before
  // |net_log_file_writer_| finishes stopping, which means this should be a
  // no-op.
  TestStateCallback start_during_stop_callback;
  net_log_file_writer_.StartNetLog(base::FilePath(),
                                   net::NetLogCaptureMode::Default(),
                                   start_during_stop_callback.callback());

  // Now run the main message loop. Since StartNetLog() will be a no-op, it will
  // simply run the state callback. There are no guarantees for when this state
  // callback will execute if StartNetLog() is called during the stopping
  // process, so the state it returns could either be stopping-log or
  // not-logging. For simplicity, the returned state will not be verified.
  start_during_stop_callback.WaitForResult();

  // The state returned by the StopNetLog() call should be the state after
  // stopping, which should indicate not-logging. Also, the capture mode should
  // be the same as the one passed to the first StartNetLog() call, not the
  // second (ignored) one.
  std::unique_ptr<base::DictionaryValue> stop_callback_state =
      stop_callback.WaitForResult();
  VerifyState(*stop_callback_state, kStateNotLoggingString, true, true,
              kCaptureModeIncludeSocketBytesString);

  // Run an additional GetState() just to make sure |net_log_file_writer_| is
  // not logging.
  VerifyState(*FileWriterGetState(), kStateNotLoggingString, true, true,
              kCaptureModeIncludeSocketBytesString);
}

}  // namespace net_log
