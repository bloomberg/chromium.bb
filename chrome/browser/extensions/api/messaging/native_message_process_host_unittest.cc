// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/scoped_handle.h"
#else
#include <unistd.h>
#endif

using content::BrowserThread;

namespace {

const char kTestMessage[] = "{\"text\": \"Hello.\"}";

base::FilePath GetTestDir() {
  base::FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  test_dir = test_dir.AppendASCII("native_messaging");
  return test_dir;
}

}  // namespace

namespace extensions {

class FakeLauncher : public NativeProcessLauncher {
 public:
  FakeLauncher(base::PlatformFile read_file, base::PlatformFile write_file)
    : read_file_(read_file),
      write_file_(write_file) {
  }

  static scoped_ptr<NativeProcessLauncher> Create(base::FilePath read_file,
                                         base::FilePath write_file) {
    int read_flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
                     base::PLATFORM_FILE_ASYNC;
    int write_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE |
                      base::PLATFORM_FILE_ASYNC;
    return scoped_ptr<NativeProcessLauncher>(new FakeLauncher(
        base::CreatePlatformFile(read_file, read_flags, NULL, NULL),
        base::CreatePlatformFile(write_file, write_flags, NULL, NULL)));
  }

  static scoped_ptr<NativeProcessLauncher> CreateWithPipeInput(
      base::PlatformFile read_pipe,
      base::FilePath write_file) {
    int write_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE |
                      base::PLATFORM_FILE_ASYNC;
    return scoped_ptr<NativeProcessLauncher>(new FakeLauncher(
        read_pipe,
        base::CreatePlatformFile(write_file, write_flags, NULL, NULL)));
  }

  virtual void Launch(const GURL& origin,
                      const std::string& native_host_name,
                      LaunchedCallback callback) const OVERRIDE {
    callback.Run(NativeProcessLauncher::RESULT_SUCCESS,
                 base::kNullProcessHandle, read_file_, write_file_);
  }

 private:
  base::PlatformFile read_file_;
  base::PlatformFile write_file_;
};

class NativeMessagingTest : public ::testing::Test,
                            public NativeMessageProcessHost::Client,
                            public base::SupportsWeakPtr<NativeMessagingTest> {
 protected:
  NativeMessagingTest()
      : current_channel_(chrome::VersionInfo::CHANNEL_DEV),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Change the user data dir so native apps will be looked for in the test
    // directory.
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, GetTestDir()));
  }

  virtual void TearDown() OVERRIDE {
    // Change the user data dir back for other tests.
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, user_data_dir_));
    if (native_message_process_host_.get()) {
      BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                                native_message_process_host_.release());
    }
    base::RunLoop().RunUntilIdle();
  }

  virtual void PostMessageFromNativeProcess(
      int port_id,
      const std::string& message) OVERRIDE  {
    last_message_ = message;

    // Parse the message.
    base::Value* parsed = base::JSONReader::Read(message);
    base::DictionaryValue* dict_value;
    if (parsed && parsed->GetAsDictionary(&dict_value)) {
      last_message_parsed_.reset(dict_value);
    } else {
      LOG(ERROR) << "Failed to parse " << message;
      last_message_parsed_.reset();
      delete parsed;
    }

    if (read_message_run_loop_)
      read_message_run_loop_->Quit();
  }

  virtual void CloseChannel(int port_id,
                            const std::string& error_message) OVERRIDE {
  }

 protected:
  std::string FormatMessage(const std::string& message) {
    Pickle pickle;
    pickle.WriteString(message);
    return std::string(const_cast<const Pickle*>(&pickle)->payload(),
                       pickle.payload_size());
  }

  base::FilePath CreateTempFileWithMessage(const std::string& message) {
    base::FilePath filename = temp_dir_.path().AppendASCII("input");
    file_util::CreateTemporaryFile(&filename);
    std::string message_with_header = FormatMessage(message);
    EXPECT_TRUE(file_util::WriteFile(
        filename, message_with_header.data(), message_with_header.size()));
    return filename;
  }

  // Force the channel to be dev.
  base::ScopedTempDir temp_dir_;
  ScopedCurrentChannel current_channel_;
  scoped_ptr<NativeMessageProcessHost> native_message_process_host_;
  base::FilePath user_data_dir_;
  scoped_ptr<base::RunLoop> read_message_run_loop_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::string last_message_;
  scoped_ptr<base::DictionaryValue> last_message_parsed_;
};

// Read a single message from a local file.
TEST_F(NativeMessagingTest, SingleSendMessageRead) {
  base::FilePath temp_output_file = temp_dir_.path().AppendASCII("output");
  base::FilePath temp_input_file = CreateTempFileWithMessage(kTestMessage);

  scoped_ptr<NativeProcessLauncher> launcher =
      FakeLauncher::Create(temp_input_file, temp_output_file).Pass();
  native_message_process_host_ = NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), kTestNativeMessagingExtensionId, "empty_app.py",
      0, launcher.Pass());
  ASSERT_TRUE(native_message_process_host_.get());
  read_message_run_loop_.reset(new base::RunLoop());
  read_message_run_loop_->RunUntilIdle();

  if (last_message_.empty()) {
    read_message_run_loop_.reset(new base::RunLoop());
    native_message_process_host_->ReadNowForTesting();
    read_message_run_loop_->Run();
  }
  EXPECT_EQ(kTestMessage, last_message_);
}

// Tests sending a single message. The message should get written to
// |temp_file| and should match the contents of single_message_request.msg.
TEST_F(NativeMessagingTest, SingleSendMessageWrite) {
  base::FilePath temp_output_file = temp_dir_.path().AppendASCII("output");

  base::PlatformFile read_file;
#if defined(OS_WIN)
  string16 pipe_name = base::StringPrintf(
      L"\\\\.\\pipe\\chrome.nativeMessaging.out.%llx", base::RandUint64());
  base::win::ScopedHandle write_handle(
      CreateNamedPipeW(pipe_name.c_str(),
                       PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                           FILE_FLAG_FIRST_PIPE_INSTANCE,
                       PIPE_TYPE_BYTE, 1, 0, 0, 5000, NULL));
  ASSERT_TRUE(write_handle);
  base::win::ScopedHandle read_handle(
      CreateFileW(pipe_name.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL));
  ASSERT_TRUE(read_handle);

  read_file = read_handle.Get();
#else  // defined(OS_WIN)
  base::PlatformFile pipe_handles[2];
  ASSERT_EQ(0, pipe(pipe_handles));
  file_util::ScopedFD read_fd(pipe_handles);
  file_util::ScopedFD write_fd(pipe_handles + 1);

  read_file = pipe_handles[0];
#endif  // !defined(OS_WIN)

  scoped_ptr<NativeProcessLauncher> launcher =
      FakeLauncher::CreateWithPipeInput(read_file, temp_output_file).Pass();
  native_message_process_host_ = NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), kTestNativeMessagingExtensionId, "empty_app.py",
      0, launcher.Pass());
  ASSERT_TRUE(native_message_process_host_.get());
  base::RunLoop().RunUntilIdle();

  native_message_process_host_->Send(kTestMessage);
  base::RunLoop().RunUntilIdle();

  std::string output;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (base::TimeTicks::Now() - start_time < TestTimeouts::action_timeout()) {
    ASSERT_TRUE(base::ReadFileToString(temp_output_file, &output));
    if (!output.empty())
      break;
    base::PlatformThread::YieldCurrentThread();
  }

  EXPECT_EQ(FormatMessage(kTestMessage), output);
}

// Test send message with a real client. The client just echo's back the text
// it received.
TEST_F(NativeMessagingTest, EchoConnect) {
  base::FilePath manifest_path = temp_dir_.path().AppendASCII(
      std::string(kTestNativeMessagingHostName) + ".json");
  ASSERT_NO_FATAL_FAILURE(CreateTestNativeHostManifest(manifest_path));

  std::string hosts_option = base::StringPrintf(
      "%s=%s", extensions::kTestNativeMessagingHostName,
      manifest_path.AsUTF8Unsafe().c_str());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kNativeMessagingHosts, hosts_option);

  native_message_process_host_ = NativeMessageProcessHost::Create(
      gfx::NativeView(), AsWeakPtr(), kTestNativeMessagingExtensionId,
      kTestNativeMessagingHostName, 0);
  ASSERT_TRUE(native_message_process_host_.get());

  native_message_process_host_->Send("{\"text\": \"Hello.\"}");
  read_message_run_loop_.reset(new base::RunLoop());
  read_message_run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);

  std::string expected_url = std::string("chrome-extension://") +
      kTestNativeMessagingExtensionId + "/";
  int id;
  EXPECT_TRUE(last_message_parsed_->GetInteger("id", &id));
  EXPECT_EQ(1, id);
  std::string text;
  EXPECT_TRUE(last_message_parsed_->GetString("echo.text", &text));
  EXPECT_EQ("Hello.", text);
  std::string url;
  EXPECT_TRUE(last_message_parsed_->GetString("caller_url", &url));
  EXPECT_EQ(expected_url, url);


  native_message_process_host_->Send("{\"foo\": \"bar\"}");
  read_message_run_loop_.reset(new base::RunLoop());
  read_message_run_loop_->Run();
  EXPECT_TRUE(last_message_parsed_->GetInteger("id", &id));
  EXPECT_EQ(2, id);
  EXPECT_TRUE(last_message_parsed_->GetString("echo.foo", &text));
  EXPECT_EQ("bar", text);
  EXPECT_TRUE(last_message_parsed_->GetString("caller_url", &url));
  EXPECT_EQ(expected_url, url);
}

}  // namespace extensions
