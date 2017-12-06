// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_manager.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

namespace {
struct PeerConnectionKey {
  int render_process_id;
  int lid;  // Renderer-local PeerConnection ID.
};

enum class ExpectedResult : bool { kFailure = false, kSuccess = true };
}  // namespace

class WebRtcEventLogManagerTest : public ::testing::Test {
 protected:
  WebRtcEventLogManagerTest()
      : run_loop_(std::make_unique<base::RunLoop>()),
        manager_(new WebRtcEventLogManager) {
    EXPECT_TRUE(
        base::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &base_dir_));
    if (base_dir_.empty()) {
      EXPECT_TRUE(false);
      return;
    }
    base_path_ = base_dir_.Append(FILE_PATH_LITERAL("webrtc_event_log"));
  }

  ~WebRtcEventLogManagerTest() override {
    DestroyUnitUnderTest();
    if (!base_dir_.empty()) {
      EXPECT_TRUE(base::DeleteFile(base_dir_, true));
    }
  }

  void DestroyUnitUnderTest() {
    if (manager_ != nullptr) {
      delete manager_;  // Raw pointer; see definition for rationale.
      manager_ = nullptr;
    }
  }

  void WaitForReply() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop);  // Allow re-blocking.
  }

  void ExpectBoolReply(bool expected_value, bool value) {
    EXPECT_EQ(expected_value, value);
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(bool)> ExpectBoolReplyClosure(bool expected_value) {
    return base::BindOnce(&WebRtcEventLogManagerTest::ExpectBoolReply,
                          base::Unretained(this), expected_value);
  }

  // With partial binding, we'll get a closure that will write the reply
  // into a predefined destination. (Diverging from the style-guide by putting
  // an output parameter first is necessary for partial binding.)
  void OnFilePathReply(base::FilePath* out_path, base::FilePath value) {
    *out_path = value;
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(base::FilePath)> FilePathReplyClosure(
      base::FilePath* file_path) {
    return base::BindOnce(&WebRtcEventLogManagerTest::OnFilePathReply,
                          base::Unretained(this), file_path);
  }

  base::FilePath LocalWebRtcEventLogStart(int render_process_id,
                                          int lid,
                                          const base::FilePath& base_path,
                                          size_t max_file_size) {
    base::FilePath file_path;
    manager_->LocalWebRtcEventLogStart(render_process_id, lid, base_path,
                                       max_file_size,
                                       FilePathReplyClosure(&file_path));
    WaitForReply();
    return file_path;
  }

  void LocalWebRtcEventLogStop(int render_process_id,
                               int lid,
                               ExpectedResult expected_result) {
    const bool expected_result_bool = static_cast<bool>(expected_result);
    manager_->LocalWebRtcEventLogStop(
        render_process_id, lid, ExpectBoolReplyClosure(expected_result_bool));
    WaitForReply();
  }

  void OnWebRtcEventLogWrite(int render_process_id,
                             int lid,
                             const std::string& output,
                             ExpectedResult expected_result) {
    bool expected_result_bool = static_cast<bool>(expected_result);
    manager_->OnWebRtcEventLogWrite(
        render_process_id, lid, output,
        ExpectBoolReplyClosure(expected_result_bool));
    WaitForReply();
  }

  void FreezeClockAt(const base::Time::Exploded& frozen_time_exploded) {
    base::Time frozen_time;
    ASSERT_TRUE(
        base::Time::FromLocalExploded(frozen_time_exploded, &frozen_time));
    frozen_clock_.SetNow(frozen_time);
    manager_->InjectClockForTesting(&frozen_clock_);
  }

  // Common default values.
  static constexpr int kRenderProcessId = 23;
  static constexpr int kLocalPeerConnectionId = 478;
  static constexpr size_t kMaxFileSizeBytes = 50000;

  // Testing utilities.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<base::RunLoop> run_loop_;  // New instance allows reblocking.
  base::SimpleTestClock frozen_clock_;

  // Unit under test. (Cannot be a unique_ptr because of its private ctor/dtor.)
  WebRtcEventLogManager* manager_;

  base::FilePath base_dir_;   // The directory where we'll save log files.
  base::FilePath base_path_;  // base_dir_ +  log files' name prefix.
};

TEST_F(WebRtcEventLogManagerTest, LocalLogCreateEmptyFile) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, "");
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreateAndWriteToFile) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  const std::string log = "To strive, to seek, to find, and not to yield.";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log,
                        ExpectedResult::kSuccess);

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleWritesToSameFile) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  const std::string logs[] = {"Old age hath yet his honour and his toil;",
                              "Death closes all: but something ere the end,",
                              "Some work of noble note, may yet be done,",
                              "Not unbecoming men that strove with Gods."};

  for (const std::string& log : logs) {
    OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log,
                          ExpectedResult::kSuccess);
  }

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents,
            std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFileSizeLimitNotExceeded) {
  const std::string log = "There lies the port; the vessel puffs her sail:";
  const size_t file_size_limit = log.length() / 2;

  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, file_size_limit);
  ASSERT_FALSE(file_path.empty());

  // A failure is reported, because not everything could be written.
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log,
                        ExpectedResult::kFailure);

  // The file has already been closed when Write() failed, since no further
  // Write() is expected to have any effect.
  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kFailure);

  // Additional calls to Write() have no effect.
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, "ignored",
                        ExpectedResult::kFailure);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log.substr(0, file_size_limit));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityOverUnlimitedFileSizes) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_,
      WebRtcEventLogManager::kUnlimitedFileSize);
  ASSERT_FALSE(file_path.empty());

  const std::string log1 = "Who let the dogs out?";
  const std::string log2 = "Woof, woof, woof, woof, woof!";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log1,
                        ExpectedResult::kSuccess);
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log2,
                        ExpectedResult::kSuccess);

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log1 + log2);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogNoWriteAfterLogStop) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  const std::string log_before = "log_before_stop";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log_before,
                        ExpectedResult::kSuccess);

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  const std::string log_after = "log_after_stop";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log_after,
                        ExpectedResult::kFailure);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log_before);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogOnlyWritesTheLogsAfterStart) {
  // Calls to Write() before Start() are ignored.
  const std::string log1 = "The lights begin to twinkle from the rocks:";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log1,
                        ExpectedResult::kFailure);
  ASSERT_TRUE(base::IsDirectoryEmpty(base_dir_));

  // Calls after Start() have an effect. The calls to Write() from before
  // Start() are not remembered.
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  const std::string log2 = "The long day wanes: the slow moon climbs: the deep";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log2,
                        ExpectedResult::kSuccess);
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log2);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogStopBeforeStartHasNoEffect) {
  // Calls to Stop() before Start() are ignored.
  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kFailure);
  ASSERT_TRUE(base::IsDirectoryEmpty(base_dir_));

  // The Stop() before does not leave any bad state behind. We can still
  // Start() the log, Write() to it and Close() it.
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  const std::string log = "To err is canine; to forgive, feline.";
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log,
                        ExpectedResult::kSuccess);
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, log);
}

// Note: This test also covers the scenario LocalLogExistingFilesNotOverwritten,
// which is therefore not explicitly tested.
TEST_F(WebRtcEventLogManagerTest, LocalLogRestartedLogCreatesNewFile) {
  const std::vector<std::string> logs = {"<setup>", "<punchline>", "encore"};
  std::vector<base::FilePath> file_paths;
  for (size_t i = 0; i < logs.size(); i++) {
    file_paths.push_back(
        LocalWebRtcEventLogStart(kRenderProcessId, kLocalPeerConnectionId,
                                 base_path_, kMaxFileSizeBytes));
    ASSERT_FALSE(file_paths.back().empty());

    OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, logs[i],
                          ExpectedResult::kSuccess);

    LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                            ExpectedResult::kSuccess);
  }

  for (size_t i = 0; i < logs.size(); i++) {
    std::string file_contents;
    EXPECT_TRUE(base::ReadFileToString(file_paths[i], &file_contents));
    EXPECT_EQ(file_contents, logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleActiveFiles) {
  const std::vector<PeerConnectionKey> keys = {{1, 2}, {2, 1}, {3, 4},
                                               {4, 3}, {5, 5}, {6, 7}};
  std::vector<std::string> logs;
  std::vector<base::FilePath> file_paths;

  for (const auto& key : keys) {
    file_paths.push_back(LocalWebRtcEventLogStart(
        key.render_process_id, key.lid, base_path_, kMaxFileSizeBytes));
    ASSERT_FALSE(file_paths.back().empty());
  }

  for (size_t i = 0; i < keys.size(); i++) {
    logs.emplace_back(std::to_string(kRenderProcessId) +
                      std::to_string(kLocalPeerConnectionId));
    OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i],
                          ExpectedResult::kSuccess);
  }

  for (size_t i = 0; i < keys.size(); i++) {
    LocalWebRtcEventLogStop(keys[i].render_process_id, keys[i].lid,
                            ExpectedResult::kSuccess);
  }

  for (size_t i = 0; i < keys.size(); i++) {
    std::string file_contents;
    EXPECT_TRUE(base::ReadFileToString(file_paths[i], &file_contents));
    EXPECT_EQ(file_contents, logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityDestructorWithActiveFile) {
  // We don't actually test that the file was closed, because this behavior
  // is not supported - WebRtcEventLogManager's destructor may never be called,
  // except for in unit-tests.
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());
  DestroyUnitUnderTest();  // Explicitly show destruction does not crash.
}

// TODO(eladalon): Re-enable after handling https://crbug.com/786374
TEST_F(WebRtcEventLogManagerTest, DISABLED_LocalLogIllegalPath) {
  const base::FilePath illegal_path(FILE_PATH_LITERAL(":!@#$%|`^&*\\/"));
  const base::FilePath file_path =
      LocalWebRtcEventLogStart(kRenderProcessId, kLocalPeerConnectionId,
                               illegal_path, kMaxFileSizeBytes);
  EXPECT_TRUE(file_path.empty());
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir_));
}

#if defined(OS_POSIX)
TEST_F(WebRtcEventLogManagerTest, LocalLogLegalPathWithoutPermissionsSanity) {
  // Remove writing permissions from the entire directory.
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(base_dir_, &permissions));
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  permissions &= ~write_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(base_dir_, permissions));

  // Start() has no effect (but is handled gracefully).
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  EXPECT_TRUE(file_path.empty());
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir_));

  // Write() has no effect (but is handled gracefully).
  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId,
                        "Why did the chicken cross the road?",
                        ExpectedResult::kFailure);
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir_));

  // Stop() has no effect (but is handled gracefully).
  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kFailure);
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir_));
}
#endif

TEST_F(WebRtcEventLogManagerTest, LocalLogEmptyStringHandledGracefully) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "encore"};
  for (const auto& log : logs) {
    OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, log,
                          ExpectedResult::kSuccess);
  }

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents,
            std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

// The logs would typically be binary. However, the other tests only cover ASCII
// characters, for readability. This test shows that this is not a problem.
TEST_F(WebRtcEventLogManagerTest, LocalLogAllPossibleCharacters) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());

  std::string all_chars;
  for (size_t i = 0; i < 256; i++) {
    all_chars += static_cast<uint8_t>(i);
  }

  OnWebRtcEventLogWrite(kRenderProcessId, kLocalPeerConnectionId, all_chars,
                        ExpectedResult::kSuccess);

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, all_chars);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFilenameMatchesExpectedFormat) {
  using StringType = base::FilePath::StringType;

  const base::Time::Exploded frozen_time_exploded{
      2017,  // Four digit year "2007"
      9,     // 1-based month (values 1 = January, etc.)
      3,     // 0-based day of week (0 = Sunday, etc.)
      6,     // 1-based day of month (1-31)
      10,    // Hour within the current day (0-23)
      43,    // Minute within the current hour (0-59)
      29,    // Second within the current minute.
      0      // Milliseconds within the current second (0-999)
  };
  ASSERT_TRUE(frozen_time_exploded.HasValidValues());
  FreezeClockAt(frozen_time_exploded);

  const StringType user_defined = FILE_PATH_LITERAL("user_defined");
  const base::FilePath base_path = base_dir_.Append(user_defined);

  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path, kMaxFileSizeBytes);

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path = base_path;
  expected_path = base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(kRenderProcessId) +
      FILE_PATH_LITERAL("_") + IntToStringType(kLocalPeerConnectionId));
  expected_path = expected_path.AddExtension(FILE_PATH_LITERAL("log"));

  EXPECT_EQ(file_path, expected_path);
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogFilenameMatchesExpectedFormatRepeatedFilename) {
  using StringType = base::FilePath::StringType;

  const base::Time::Exploded frozen_time_exploded{
      2017,  // Four digit year "2007"
      9,     // 1-based month (values 1 = January, etc.)
      3,     // 0-based day of week (0 = Sunday, etc.)
      6,     // 1-based day of month (1-31)
      10,    // Hour within the current day (0-23)
      43,    // Minute within the current hour (0-59)
      29,    // Second within the current minute.
      0      // Milliseconds within the current second (0-999)
  };
  ASSERT_TRUE(frozen_time_exploded.HasValidValues());
  FreezeClockAt(frozen_time_exploded);

  const StringType user_defined_portion =
      FILE_PATH_LITERAL("user_defined_portion");
  const base::FilePath base_path = base_dir_.Append(user_defined_portion);

  const base::FilePath file_path_1 = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path, kMaxFileSizeBytes);

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path_1 = base_path;
  expected_path_1 = base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(kRenderProcessId) +
      FILE_PATH_LITERAL("_") + IntToStringType(kLocalPeerConnectionId));
  expected_path_1 = expected_path_1.AddExtension(FILE_PATH_LITERAL("log"));

  ASSERT_EQ(file_path_1, expected_path_1);

  LocalWebRtcEventLogStop(kRenderProcessId, kLocalPeerConnectionId,
                          ExpectedResult::kSuccess);

  const base::FilePath file_path_2 = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path, kMaxFileSizeBytes);

  const base::FilePath expected_path_2 =
      expected_path_1.InsertBeforeExtension(FILE_PATH_LITERAL(" (1)"));

  // Focus of the test - starting the same log again produces a new file,
  // with an expected new filename.
  ASSERT_EQ(file_path_2, expected_path_2);
}


// TODO(eladalon): Fix the failure (Win7) and re-enable.
// https://crbug.com/791022
TEST_F(WebRtcEventLogManagerTest, DISABLED_LocalLogMayNotBeStartedTwice) {
  const base::FilePath file_path = LocalWebRtcEventLogStart(
      kRenderProcessId, kLocalPeerConnectionId, base_path_, kMaxFileSizeBytes);
  ASSERT_FALSE(file_path.empty());
  // Known issue - this always passes (whether LocalWebRtcEventLogStart crashes
  // or not). http://crbug.com/787809
  EXPECT_DEATH_IF_SUPPORTED(
      LocalWebRtcEventLogStart(kRenderProcessId, kLocalPeerConnectionId,
                               base_path_, kMaxFileSizeBytes),
      "");
}

}  // namespace content
