// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/fallback_crash_handler_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

namespace crash_reporter {

namespace {

class FallbackCrashHandlerWinTest : public testing::Test {
 public:
  FallbackCrashHandlerWinTest() : self_handle_(base::kNullProcessHandle) {
    RtlCaptureContext(&context_);
    memset(&exception_, 0, sizeof(exception_));
    exception_.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;

    exception_ptrs_.ExceptionRecord = &exception_;
    exception_ptrs_.ContextRecord = &context_;
  }

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());

    // Open a handle to our own process.
    const DWORD kAccessMask =
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE;
    self_handle_ = OpenProcess(kAccessMask, FALSE, base::GetCurrentProcId());
    DWORD err = GetLastError();
    EXPECT_NE(base::kNullProcessHandle, self_handle_) << "GetLastError: "
                                                      << err;
  }

  void TearDown() override {
    if (self_handle_ != base::kNullProcessHandle) {
      CloseHandle(self_handle_);
      self_handle_ = base::kNullProcessHandle;
    }
  }

  std::string ExcPtrsAsString() const {
    return base::Uint64ToString(reinterpret_cast<uintptr_t>(&exception_ptrs_));
  };

  std::string SelfHandleAsString() const {
    return base::UintToString(base::win::HandleToUint32(self_handle_));
  };

  void CreateDatabase() {
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::InitializeWithoutCreating(
            database_dir_.GetPath());
  }

 protected:
  CONTEXT context_;
  EXCEPTION_RECORD exception_;
  EXCEPTION_POINTERS exception_ptrs_;

  base::ProcessHandle self_handle_;
  base::ScopedTempDir database_dir_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCrashHandlerWinTest);
};

}  // namespace

TEST_F(FallbackCrashHandlerWinTest, ParseCommandLine) {
  FallbackCrashHandler handler;

  // An empty command line shouldn't work.
  base::CommandLine cmd_line(base::FilePath(L"empty"));
  ASSERT_FALSE(handler.ParseCommandLine(cmd_line));

  cmd_line.AppendSwitchPath("database", database_dir_.GetPath());
  cmd_line.AppendSwitchASCII("exception-pointers", ExcPtrsAsString());
  cmd_line.AppendSwitchASCII("process", SelfHandleAsString());

  // Thread missing, still should fail.
  ASSERT_FALSE(handler.ParseCommandLine(cmd_line));

  cmd_line.AppendSwitchASCII(
      "thread", base::UintToString(base::PlatformThread::CurrentId()));

  // Should succeed with a fully populated command line.
  // Because of how handle ownership is guarded, we have to "disown" it before
  // the handler takes it over.
  EXPECT_TRUE(handler.ParseCommandLine(cmd_line));
  self_handle_ = base::kNullProcessHandle;
}

TEST_F(FallbackCrashHandlerWinTest, GenerateCrashDump) {
  FallbackCrashHandler handler;

  base::CommandLine cmd_line(base::FilePath(L"empty"));
  cmd_line.AppendSwitchPath("database", database_dir_.GetPath());
  cmd_line.AppendSwitchASCII("exception-pointers", ExcPtrsAsString());

  cmd_line.AppendSwitchASCII("process", SelfHandleAsString());
  cmd_line.AppendSwitchASCII(
      "thread", base::UintToString(base::PlatformThread::CurrentId()));

  // Because how handle ownership is guarded, we have to "disown" this before
  // the handler takes it over.
  ASSERT_TRUE(handler.ParseCommandLine(cmd_line));
  self_handle_ = base::kNullProcessHandle;

  const char kProduct[] = "SomeProduct";
  const char kVersion[] = "1.2.3.6";
  const char kChannel[] = "canary";
  const char kProcessType[] = "Test";

  // TODO(siggi): If this ends up being flakey, spin the testing out to a
  //     a separate process, which can in turn use the
  //     FallbackCrashHandlerLauncher class to spin a third process to grab
  //     a dump.
  EXPECT_TRUE(
      handler.GenerateCrashDump(kProduct, kVersion, kChannel, kProcessType));

  // Validate that the database contains one valid crash dump.
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(
          database_dir_.GetPath());

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  ASSERT_EQ(crashpad::CrashReportDatabase::kNoError,
            database->GetPendingReports(&reports));

  EXPECT_EQ(1U, reports.size());

  // Validate crashpad can read the produced minidump.
  crashpad::FileReader minidump_file_reader;
  ASSERT_TRUE(minidump_file_reader.Open(reports[0].file_path));

  crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
  ASSERT_TRUE(minidump_process_snapshot.Initialize(&minidump_file_reader));

  crashpad::UUID expected_client_id;
  ASSERT_TRUE(database->GetSettings()->GetClientID(&expected_client_id));

  // Validate that the CrashpadInfo in the report contains the same basic
  // info, as does the database.
  crashpad::UUID client_id;
  minidump_process_snapshot.ClientID(&client_id);
  EXPECT_EQ(expected_client_id, client_id);

  crashpad::UUID report_id;
  minidump_process_snapshot.ReportID(&report_id);
  EXPECT_EQ(reports[0].uuid, report_id);

  std::map<std::string, std::string> parameters =
      minidump_process_snapshot.AnnotationsSimpleMap();
  auto it = parameters.find("prod");
  EXPECT_NE(parameters.end(), it);
  EXPECT_EQ(kProduct, it->second);

  it = parameters.find("ver");
  EXPECT_NE(parameters.end(), it);
  EXPECT_EQ(kVersion, it->second);

  it = parameters.find("channel");
  EXPECT_NE(parameters.end(), it);
  EXPECT_EQ(kChannel, it->second);

  it = parameters.find("plat");
  EXPECT_NE(parameters.end(), it);
#if defined(ARCH_CPU_64_BITS)
  EXPECT_EQ("Win64", it->second);
#else
  EXPECT_EQ("Win32", it->second);
#endif

  it = parameters.find("ptype");
  EXPECT_NE(parameters.end(), it);
  EXPECT_EQ(kProcessType, it->second);
}

}  // namespace crash_reporter
