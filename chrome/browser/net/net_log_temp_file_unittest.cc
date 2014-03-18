// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_log_temp_file.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class TestNetLogTempFile : public NetLogTempFile {
 public:
  explicit TestNetLogTempFile(ChromeNetLog* chrome_net_log)
      : NetLogTempFile(chrome_net_log),
        lie_about_net_export_log_directory_(false),
        lie_about_file_existence_(false) {
  }

  // NetLogTempFile implementation:
  virtual bool GetNetExportLogDirectory(base::FilePath* path) OVERRIDE {
    if (lie_about_net_export_log_directory_)
      return false;
    return NetLogTempFile::GetNetExportLogDirectory(path);
  }

  virtual bool NetExportLogExists() OVERRIDE {
    if (lie_about_file_existence_)
      return false;
    return NetLogTempFile::NetExportLogExists();
  }

  void set_lie_about_net_export_log_directory(
      bool lie_about_net_export_log_directory) {
    lie_about_net_export_log_directory_ = lie_about_net_export_log_directory;
  }

  void set_lie_about_file_existence(bool lie_about_file_existence) {
    lie_about_file_existence_ = lie_about_file_existence;
  }

 private:
  bool lie_about_net_export_log_directory_;
  bool lie_about_file_existence_;
};

class NetLogTempFileTest : public ::testing::Test {
 public:
  NetLogTempFileTest()
      : net_log_(new ChromeNetLog),
        net_log_temp_file_(new TestNetLogTempFile(net_log_.get())),
        file_user_blocking_thread_(BrowserThread::FILE_USER_BLOCKING,
                                   &message_loop_) {
  }

  // ::testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    // Get a temporary file name for unit tests.
    base::FilePath net_log_dir;
    ASSERT_TRUE(net_log_temp_file_->GetNetExportLogDirectory(&net_log_dir));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(net_log_dir, &net_export_log_));

    net_log_temp_file_->log_filename_ = net_export_log_.BaseName().value();

    // CreateTemporaryFileInDir may return a legacy 8.3 file name on windows.
    // Need to use the original directory name for string comparisons.
    ASSERT_TRUE(net_log_temp_file_->GetNetExportLog());
    net_export_log_ = net_log_temp_file_->log_path_;
    ASSERT_FALSE(net_export_log_.empty());
  }

  virtual void TearDown() OVERRIDE {
    // Delete the temporary file we have created.
    ASSERT_TRUE(base::DeleteFile(net_export_log_, false));
  }

  std::string GetStateString() const {
    scoped_ptr<base::DictionaryValue> dict(net_log_temp_file_->GetState());
    std::string state;
    EXPECT_TRUE(dict->GetString("state", &state));
    return state;
  }

  std::string GetLogTypeString() const {
    scoped_ptr<base::DictionaryValue> dict(net_log_temp_file_->GetState());
    std::string log_type;
    EXPECT_TRUE(dict->GetString("logType", &log_type));
    return log_type;
  }

  // Make sure the export file has been created and is non-empty, as net
  // constants will always be written to it on creation.
  void VerifyNetExportLog() {
    EXPECT_EQ(net_export_log_, net_log_temp_file_->log_path_);
    EXPECT_TRUE(base::PathExists(net_export_log_));

    int64 file_size;
    // base::GetFileSize returns proper file size on open handles.
    EXPECT_TRUE(base::GetFileSize(net_export_log_, &file_size));
    EXPECT_GT(file_size, 0);
  }

  // Verify state and GetFilePath return correct values if EnsureInit() fails.
  void VerifyFilePathAndStateAfterEnsureInitFailure() {
    EXPECT_EQ("UNINITIALIZED", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_UNINITIALIZED,
              net_log_temp_file_->state());

    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
  }

  // When we lie in NetExportLogExists, make sure state and GetFilePath return
  // correct values.
  void VerifyFilePathAndStateAfterEnsureInit() {
    EXPECT_EQ("NOT_LOGGING", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("NONE", GetLogTypeString());
    EXPECT_EQ(NetLogTempFile::LOG_TYPE_NONE, net_log_temp_file_->log_type());

    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
    EXPECT_FALSE(net_log_temp_file_->NetExportLogExists());
  }

  // Make sure the export file has been successfully initialized.
  void VerifyFileAndStateAfterDoStart() {
    EXPECT_EQ("LOGGING", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("NORMAL", GetLogTypeString());
    EXPECT_EQ(NetLogTempFile::LOG_TYPE_NORMAL, net_log_temp_file_->log_type());

    // Check GetFilePath returns false, if we are still writing to file.
    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));

    VerifyNetExportLog();
    EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, net_log_->GetLogLevel());
  }

  // Make sure the export file has been successfully initialized.
  void VerifyFileAndStateAfterDoStartStripPrivateData() {
    EXPECT_EQ("LOGGING", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("STRIP_PRIVATE_DATA", GetLogTypeString());
    EXPECT_EQ(NetLogTempFile::LOG_TYPE_STRIP_PRIVATE_DATA,
              net_log_temp_file_->log_type());

    // Check GetFilePath returns false, if we are still writing to file.
    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));

    VerifyNetExportLog();
    EXPECT_EQ(net::NetLog::LOG_STRIP_PRIVATE_DATA, net_log_->GetLogLevel());
  }

  // Make sure the export file has been successfully initialized.
  void VerifyFileAndStateAfterDoStop() {
    EXPECT_EQ("NOT_LOGGING", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("NORMAL", GetLogTypeString());
    EXPECT_EQ(NetLogTempFile::LOG_TYPE_NORMAL, net_log_temp_file_->log_type());

    base::FilePath net_export_file_path;
    EXPECT_TRUE(net_log_temp_file_->GetFilePath(&net_export_file_path));
    EXPECT_TRUE(base::PathExists(net_export_file_path));
    EXPECT_EQ(net_export_log_, net_export_file_path);

    VerifyNetExportLog();
  }

  scoped_ptr<ChromeNetLog> net_log_;
  // |net_log_temp_file_| is initialized after |net_log_| so that it can stop
  // obvserving on destruction.
  scoped_ptr<TestNetLogTempFile> net_log_temp_file_;
  base::FilePath net_export_log_;

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread file_user_blocking_thread_;
};

TEST_F(NetLogTempFileTest, EnsureInitFailure) {
  net_log_temp_file_->set_lie_about_net_export_log_directory(true);

  EXPECT_FALSE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInitFailure();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFilePathAndStateAfterEnsureInitFailure();
}

TEST_F(NetLogTempFileTest, EnsureInitAllowStart) {
  net_log_temp_file_->set_lie_about_file_existence(true);

  EXPECT_TRUE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInit();

  // Calling EnsureInit() second time should be a no-op.
  EXPECT_TRUE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInit();
}

TEST_F(NetLogTempFileTest, EnsureInitAllowStartOrSend) {
  EXPECT_TRUE(net_log_temp_file_->EnsureInit());

  EXPECT_EQ("NOT_LOGGING", GetStateString());
  EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
  EXPECT_EQ("UNKNOWN", GetLogTypeString());
  EXPECT_EQ(NetLogTempFile::LOG_TYPE_UNKNOWN, net_log_temp_file_->log_type());
  EXPECT_EQ(net_export_log_, net_log_temp_file_->log_path_);
  EXPECT_TRUE(base::PathExists(net_export_log_));

  base::FilePath net_export_file_path;
  EXPECT_TRUE(net_log_temp_file_->GetFilePath(&net_export_file_path));
  EXPECT_TRUE(base::PathExists(net_export_file_path));
  EXPECT_EQ(net_export_log_, net_export_file_path);

  // GetFilePath should return false if NetExportLogExists() fails.
  net_log_temp_file_->set_lie_about_file_existence(true);
  EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
}

TEST_F(NetLogTempFileTest, ProcessCommandDoStartAndStop) {
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Calling DO_START second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  // Calling DO_STOP second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();
}

TEST_F(NetLogTempFileTest, DoStartClearsFile) {
  // Verify file sizes after two consecutives start/stop are the same (even if
  // we add some junk data in between).
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  int64 start_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &start_file_size));

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64 stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &stop_file_size));
  EXPECT_GE(stop_file_size, start_file_size);

  // Add some junk at the end of the file.
  std::string junk_data("Hello");
  EXPECT_GT(base::AppendToFile(
      net_export_log_, junk_data.c_str(), junk_data.size()), 0);

  int64 junk_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &junk_file_size));
  EXPECT_GT(junk_file_size, stop_file_size);

  // Execute DO_START/DO_STOP commands and make sure the file is back to the
  // size before addition of junk data.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  int64 new_start_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_start_file_size));
  EXPECT_EQ(new_start_file_size, start_file_size);

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64 new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_stop_file_size));
  EXPECT_EQ(new_stop_file_size, stop_file_size);
}

TEST_F(NetLogTempFileTest, CheckAddEvent) {
  // Add an event to |net_log_| and then test to make sure that, after we stop
  // logging, the file is larger than the file created without that event.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Get file size without the event.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64 stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &stop_file_size));

  // Perform DO_START and add an Event and then DO_STOP and then compare
  // file sizes.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Log an event.
  net_log_->AddGlobalEntry(net::NetLog::TYPE_CANCELLED);

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64 new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_stop_file_size));
  EXPECT_GE(new_stop_file_size, stop_file_size);
}
