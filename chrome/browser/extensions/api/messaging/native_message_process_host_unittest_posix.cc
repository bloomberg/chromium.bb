// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

const char kTestMessage[] = "{\"text\": \"Hello.\"}";

FilePath GetTestDir() {
  FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  test_dir = test_dir.AppendASCII("native_messaging");
  return test_dir;
}

}  // namespace

namespace extensions {

class FakeLauncher : public NativeProcessLauncher {
 public:
  FakeLauncher(FilePath read_file, FilePath write_file) {
    read_file_ = base::CreatePlatformFile(
        read_file,
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL, NULL);
    write_file_ = base::CreatePlatformFile(
        write_file,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        NULL, NULL);
  }

  virtual bool LaunchNativeProcess(
      const FilePath& path,
      base::ProcessHandle* native_process_handle,
      NativeMessageProcessHost::FileHandle* read_file,
      NativeMessageProcessHost::FileHandle* write_file) const OVERRIDE {
    *native_process_handle = base::GetCurrentProcessHandle();
    *read_file = read_file_;
    *write_file = write_file_;
    return true;
  }

 private:
  base::PlatformFile read_file_;
  base::PlatformFile write_file_;
};

class NativeMessagingTest : public ::testing::Test,
                            public NativeMessageProcessHost::Client,
                            public base::SupportsWeakPtr<NativeMessagingTest> {
 public:
  NativeMessagingTest()
      : current_channel_(chrome::VersionInfo::CHANNEL_DEV),
        native_message_process_host_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNativeMessaging);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Change the user data dir so native apps will be looked for in the test
    // directory.
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, GetTestDir()));
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &message_loop_));
    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE,
                                                      &message_loop_));
  }

  virtual void TearDown() OVERRIDE {
    // Change the user data dir back for other tests.
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, user_data_dir_));
    if (native_message_process_host_.get()) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                                native_message_process_host_.release());
    }
    message_loop_.RunUntilIdle();
  }

  virtual void PostMessageFromNativeProcess(
      int port_id,
      const std::string& message) OVERRIDE  {
    last_posted_message_ = message;
  }

  virtual void CloseChannel(int port_id, bool error) OVERRIDE {
  }

 protected:
  std::string FormatMessage(const std::string& message) {
    Pickle pickle;
    pickle.WriteString(message);
    return std::string(const_cast<const Pickle*>(&pickle)->payload(),
                       pickle.payload_size());
  }

  FilePath CreateTempFileWithMessage(const std::string& message) {
    FilePath filename = temp_dir_.path().Append("input");
    file_util::CreateTemporaryFile(&filename);
    std::string message_with_header = FormatMessage(message);
    EXPECT_TRUE(file_util::WriteFile(
        filename, message_with_header.data(), message_with_header.size()));
    return filename;
  }

  // Force the channel to be dev.
  base::ScopedTempDir temp_dir_;
  Feature::ScopedCurrentChannel current_channel_;
  scoped_ptr<NativeMessageProcessHost> native_message_process_host_;
  FilePath user_data_dir_;
  MessageLoopForIO message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;
  std::string last_posted_message_;
};

// Read a single message from a local file.
TEST_F(NativeMessagingTest, SingleSendMessageRead) {
  FilePath temp_output_file = temp_dir_.path().Append("output");
  FilePath temp_input_file = CreateTempFileWithMessage(kTestMessage);

  scoped_ptr<NativeProcessLauncher> launcher(
      new FakeLauncher(temp_input_file, temp_output_file));
  native_message_process_host_ = NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), "empty_app.py", 0, launcher.Pass());
  ASSERT_TRUE(native_message_process_host_.get());
  message_loop_.RunUntilIdle();

  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kTestMessage, last_posted_message_);
}

// Tests sending a single message. The message should get written to
// |temp_file| and should match the contents of single_message_request.msg.
TEST_F(NativeMessagingTest, SingleSendMessageWrite) {
  FilePath temp_output_file = temp_dir_.path().Append("output");
  FilePath temp_input_file = CreateTempFileWithMessage(std::string());

  scoped_ptr<NativeProcessLauncher> launcher(
      new FakeLauncher(temp_input_file, temp_output_file));
  native_message_process_host_ = NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), "empty_app.py", 0, launcher.Pass());
  ASSERT_TRUE(native_message_process_host_.get());
  message_loop_.RunUntilIdle();

  native_message_process_host_->Send(kTestMessage);
  message_loop_.RunUntilIdle();

  std::string output;
  ASSERT_TRUE(file_util::ReadFileToString(temp_output_file, &output));
  EXPECT_EQ(FormatMessage(kTestMessage), output);
}

// Disabled, see http://crbug.com/159754.
// Test send message with a real client. The client just echo's back the text
// it recieved.
TEST_F(NativeMessagingTest, DISABLED_EchoConnect) {
  native_message_process_host_ = NativeMessageProcessHost::Create(
      AsWeakPtr(), "empty_app.py", 0);
  ASSERT_TRUE(native_message_process_host_.get());
  message_loop_.RunUntilIdle();

  native_message_process_host_->Send("{\"text\": \"Hello.\"}");
  message_loop_.RunUntilIdle();

  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunUntilIdle();
  EXPECT_EQ("{\"id\": 1, \"echo\": {\"text\": \"Hello.\"}}",
            last_posted_message_);

  native_message_process_host_->Send("{\"foo\": \"bar\"}");
  message_loop_.RunUntilIdle();
  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunUntilIdle();
  EXPECT_EQ("{\"id\": 2, \"echo\": {\"foo\": \"bar\"}}", last_posted_message_);
}

}  // namespace extensions
