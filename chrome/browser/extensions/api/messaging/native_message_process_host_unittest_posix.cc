// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

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
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
        NULL, NULL);
  }

  virtual bool LaunchNativeProcess(
      const FilePath& path,
      base::ProcessHandle* native_process_handle,
      NativeMessageProcessHost::FileHandle* read_file,
      NativeMessageProcessHost::FileHandle* write_file) const OVERRIDE {
    *native_process_handle = base::kNullProcessHandle;
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
  NativeMessagingTest() : current_channel_(chrome::VersionInfo::CHANNEL_DEV) {
  }

  virtual void SetUp() {
    // Change the user data dir so native apps will be looked for in the test
    // directory.
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, GetTestDir()));
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &message_loop_));
    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE,
                                                      &message_loop_));
  }

  virtual void TearDown() {
    // Change the user data dir back for other tests.
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, user_data_dir_));
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                              native_message_process_host_);
    message_loop_.RunAllPending();
  }

  void PostMessageFromNativeProcess(int port_id, const std::string& message) {
    last_posted_message_ = message;
  }

  void CloseChannel(int port_id, bool error) {
  }

  void AcquireProcess(NativeMessageProcessHost::ScopedHost process) {
    native_message_process_host_ = process.release();
  }

 protected:
  // Force the channel to be dev.
  Feature::ScopedCurrentChannel current_channel_;
  NativeMessageProcessHost* native_message_process_host_;
  FilePath user_data_dir_;
  MessageLoopForIO message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;
  std::string last_posted_message_;
};

// Read a single message from a local file (single_message_response.msg).
TEST_F(NativeMessagingTest, SingleSendMessageRead) {
  FilePath temp_file;
  file_util::CreateTemporaryFile(&temp_file);
  FakeLauncher launcher(GetTestDir().AppendASCII("single_message_response.msg"),
                        temp_file);
  NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), "empty_app.py", "{}", 0,
      NativeMessageProcessHost::TYPE_SEND_MESSAGE_REQUEST, base::Bind(
          &NativeMessagingTest::AcquireProcess, AsWeakPtr()),
      launcher);
  message_loop_.RunAllPending();
  ASSERT_TRUE(native_message_process_host_);
  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunAllPending();
  EXPECT_EQ(last_posted_message_, "{\"text\": \"Hi There!.\"}");
  file_util::Delete(temp_file, false /* non-recursive */);
}

// Tests sending a single message. The message should get written to
// |temp_file| and should match the contents of single_message_request.msg.
TEST_F(NativeMessagingTest, SingleSendMessageWrite) {
  FilePath temp_file;
  file_util::CreateTemporaryFile(&temp_file);
  FakeLauncher launcher(GetTestDir().AppendASCII("single_message_response.msg"),
                        temp_file);
  NativeMessageProcessHost::CreateWithLauncher(
      AsWeakPtr(), "empty_app.py", "{\"text\": \"Hello.\"}", 0,
      NativeMessageProcessHost::TYPE_SEND_MESSAGE_REQUEST, base::Bind(
          &NativeMessagingTest::AcquireProcess, AsWeakPtr()),
      launcher);
  message_loop_.RunAllPending();
  ASSERT_TRUE(native_message_process_host_);

  EXPECT_TRUE(file_util::ContentsEqual(
      temp_file, GetTestDir().AppendASCII("single_message_request.msg")));

  file_util::Delete(temp_file, false /* non-recursive */);
}

// Test send message with a real client. The client just echo's back the text
// it recieved.
TEST_F(NativeMessagingTest, EchoConnect) {
  NativeMessageProcessHost::Create(
      AsWeakPtr(), "echo.py", "{\"text\": \"Hello.\"}", 0,
      NativeMessageProcessHost::TYPE_CONNECT, base::Bind(
          &NativeMessagingTest::AcquireProcess, AsWeakPtr()));
  message_loop_.RunAllPending();
  ASSERT_TRUE(native_message_process_host_);

  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunAllPending();
  EXPECT_EQ(last_posted_message_,
            "{\"id\": 1, \"echo\": {\"text\": \"Hello.\"}}");

  native_message_process_host_->Send("{\"foo\": \"bar\"}");
  message_loop_.RunAllPending();
  native_message_process_host_->ReadNowForTesting();
  message_loop_.RunAllPending();
  EXPECT_EQ(last_posted_message_, "{\"id\": 2, \"echo\": {\"foo\": \"bar\"}}");
}

}  // namespace extensions
