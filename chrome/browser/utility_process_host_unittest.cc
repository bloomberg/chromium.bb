// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/utility_process_host.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UtilityProcessHostTest : public testing::Test {
 public:
  UtilityProcessHostTest() {
  }

 protected:
  MessageLoopForIO message_loop_;
};

class TestUtilityProcessHostClient : public UtilityProcessHost::Client {
 public:
  explicit TestUtilityProcessHostClient(MessageLoop* message_loop)
      : message_loop_(message_loop), success_(false) {
  }

  // UtilityProcessHost::Client methods.
  virtual void OnProcessCrashed() {
    NOTREACHED();
  }

  virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest) {
    success_ = true;
  }

  virtual void OnUnpackExtensionFailed(const std::string& error_message) {
    NOTREACHED();
  }

  virtual void OnUnpackWebResourceSucceeded(
      const ListValue& json_data) {
    NOTREACHED();
  }

  virtual void OnUnpackWebResourceFailed(
      const std::string& error_message) {
    NOTREACHED();
  }

  bool success() {
    return success_;
  }

 private:
  MessageLoop* message_loop_;
  bool success_;
};

class TestUtilityProcessHost : public UtilityProcessHost {
 public:
  TestUtilityProcessHost(TestUtilityProcessHostClient* client,
                         MessageLoop* loop_io,
                         ResourceDispatcherHost* rdh)
      : UtilityProcessHost(rdh, client, loop_io) {
  }

 protected:
  virtual FilePath GetUtilityProcessCmd() {
    FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    exe_path = exe_path.AppendASCII(WideToASCII(
        chrome::kHelperProcessExecutablePath));
    return exe_path;
  }

  virtual bool UseSandbox() {
    return false;
  }

 private:
};

class ProcessClosedObserver : public NotificationObserver {
 public:
  explicit ProcessClosedObserver(MessageLoop* message_loop)
      : message_loop_(message_loop) {
    registrar_.Add(this, NotificationType::CHILD_PROCESS_HOST_DISCONNECTED,
                   NotificationService::AllSources());
  }

  void RunUntilClose(int child_id) {
    child_id_ = child_id;
    observed_ = false;
    message_loop_->Run();
    DCHECK(observed_);
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::CHILD_PROCESS_HOST_DISCONNECTED);
    ChildProcessInfo* info = Details<ChildProcessInfo>(details).ptr();
    if (info->id() == child_id_) {
      observed_ = true;
      message_loop_->Quit();
    }
  }

  MessageLoop* message_loop_;
  NotificationRegistrar registrar_;
  int child_id_;
  bool observed_;
};

#if !defined(OS_POSIX)
// We should not run this on linux (crbug.com/22703) or MacOS (crbug.com/8102)
// until problems related to autoupdate are fixed.
TEST_F(UtilityProcessHostTest, ExtensionUnpacker) {
  // Copy the test extension into a temp dir and install from the temp dir.
  FilePath extension_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_file));
  extension_file = extension_file.AppendASCII("extensions")
                                 .AppendASCII("theme.crx");
  FilePath temp_extension_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &temp_extension_dir));
  temp_extension_dir = temp_extension_dir.AppendASCII("extension_test");
  ASSERT_TRUE(file_util::CreateDirectory(temp_extension_dir));
  ASSERT_TRUE(file_util::CopyFile(extension_file,
                                  temp_extension_dir.AppendASCII("theme.crx")));

  scoped_refptr<TestUtilityProcessHostClient> client(
      new TestUtilityProcessHostClient(&message_loop_));
  ResourceDispatcherHost rdh(NULL);
  TestUtilityProcessHost* process_host =
      new TestUtilityProcessHost(client.get(), &message_loop_, &rdh);
  // process_host will delete itself when it's done.
  ProcessClosedObserver observer(&message_loop_);
  process_host->StartExtensionUnpacker(
      temp_extension_dir.AppendASCII("theme.crx"));
  observer.RunUntilClose(process_host->id());
  EXPECT_TRUE(client->success());

  // Clean up the temp dir.
  file_util::Delete(temp_extension_dir, true);
}
#endif

}  // namespace
