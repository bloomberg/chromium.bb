// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_manager.h"

#include <algorithm>
#include <list>
#include <memory>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_remote_event_log_manager.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(eladalon): Add unit tests for incognito mode. https://crbug.com/775415

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;

using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

namespace {

const int kMaxActiveRemoteLogFiles =
    static_cast<int>(kMaxActiveRemoteBoundWebRtcEventLogs);
const int kMaxPendingRemoteLogFiles =
    static_cast<int>(kMaxPendingRemoteBoundWebRtcEventLogs);

// This implementation does not upload files, nor prtends to have finished an
// upload. Most importantly, it does not get rid of the locally-stored log file
// after finishing a simulated upload; this is useful because it keeps the file
// on disk, where unit tests may inspect it.
class NullWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  ~NullWebRtcEventLogUploader() override = default;

  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    ~Factory() override = default;

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const base::FilePath& log_file,
        WebRtcEventLogUploaderObserver* observer) override {
      return std::make_unique<NullWebRtcEventLogUploader>();
    }
  };
};

}  // namespace

class WebRtcEventLogManagerTest : public ::testing::TestWithParam<bool> {
 public:
  WebRtcEventLogManagerTest()
      : run_loop_(std::make_unique<base::RunLoop>()),
        uploader_run_loop_(std::make_unique<base::RunLoop>()),
        manager_(WebRtcEventLogManager::CreateSingletonInstance()) {
    EXPECT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL(""),
                                             &local_logs_base_dir_));
    if (local_logs_base_dir_.empty()) {
      EXPECT_TRUE(false);
      return;
    }

    local_logs_base_path_ =
        local_logs_base_dir_.Append(FILE_PATH_LITERAL("local_event_logs"));
  }

  void SetUp() override {
    SetWebRtcEventLogUploaderFactoryForTesting(
        std::make_unique<NullWebRtcEventLogUploader::Factory>());
    browser_context_ = CreateBrowserContext();
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
  }

  ~WebRtcEventLogManagerTest() override {
    if (manager_) {
      WaitForPendingTasks();
      DestroyUnitUnderTest();
    }

    if (!local_logs_base_dir_.empty()) {
      EXPECT_TRUE(base::DeleteFile(local_logs_base_dir_, true));
    }

    // Guard against unexpected state changes.
    EXPECT_TRUE(webrtc_state_change_instructions_.empty());
  }

  void DestroyUnitUnderTest() {
    SetLocalLogsObserver(nullptr);
    manager_.reset();
  }

  void WaitForReply() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop);  // Allow re-blocking.
  }

  void VoidReply() { run_loop_->QuitWhenIdle(); }

  base::OnceClosure VoidReplyClosure() {
    return base::BindOnce(&WebRtcEventLogManagerTest::VoidReply,
                          base::Unretained(this));
  }

  void BoolReply(bool* output, bool value) {
    *output = value;
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(bool)> BoolReplyClosure(bool* output) {
    return base::BindOnce(&WebRtcEventLogManagerTest::BoolReply,
                          base::Unretained(this), output);
  }

  void BoolPairReply(std::pair<bool, bool>* output,
                     std::pair<bool, bool> value) {
    *output = value;
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(std::pair<bool, bool>)> BoolPairReplyClosure(
      std::pair<bool, bool>* output) {
    return base::BindOnce(&WebRtcEventLogManagerTest::BoolPairReply,
                          base::Unretained(this), output);
  }

  bool PeerConnectionAdded(int render_process_id, int lid) {
    bool result;
    manager_->PeerConnectionAdded(render_process_id, lid,
                                  BoolReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool PeerConnectionRemoved(int render_process_id, int lid) {
    bool result;
    manager_->PeerConnectionRemoved(render_process_id, lid,
                                    BoolReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool EnableLocalLogging(
      size_t max_size_bytes = kWebRtcEventLogManagerUnlimitedFileSize) {
    return EnableLocalLogging(local_logs_base_path_, max_size_bytes);
  }

  bool EnableLocalLogging(
      base::FilePath local_logs_base_path,
      size_t max_size_bytes = kWebRtcEventLogManagerUnlimitedFileSize) {
    bool result;
    manager_->EnableLocalLogging(local_logs_base_path, max_size_bytes,
                                 BoolReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool DisableLocalLogging() {
    bool result;
    manager_->DisableLocalLogging(BoolReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool StartRemoteLogging(int render_process_id,
                          int lid,
                          size_t max_size_bytes = kArbitraryVeryLargeFileSize) {
    bool result;
    manager_->StartRemoteLogging(render_process_id, lid, max_size_bytes,
                                 BoolReplyClosure(&result));
    WaitForReply();
    return result;
  }

  void SetLocalLogsObserver(WebRtcLocalEventLogsObserver* observer) {
    manager_->SetLocalLogsObserver(observer, VoidReplyClosure());
    WaitForReply();
  }

  void SetRemoteLogsObserver(WebRtcRemoteEventLogsObserver* observer) {
    manager_->SetRemoteLogsObserver(observer, VoidReplyClosure());
    WaitForReply();
  }

  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> factory) {
    // TODO(eladalon): Discuss with Guido and resolve this TODO before landing:
    // Would it be preferable to make this (and similar SetXForTesting)
    // functions operate synchronously?
    manager_->SetWebRtcEventLogUploaderFactoryForTesting(std::move(factory));
  }

  std::pair<bool, bool> OnWebRtcEventLogWrite(int render_process_id,
                                              int lid,
                                              const std::string& message) {
    std::pair<bool, bool> result;
    manager_->OnWebRtcEventLogWrite(render_process_id, lid, message,
                                    BoolPairReplyClosure(&result));
    WaitForReply();
    return result;
  }

  void FreezeClockAt(const base::Time::Exploded& frozen_time_exploded) {
    base::Time frozen_time;
    ASSERT_TRUE(
        base::Time::FromLocalExploded(frozen_time_exploded, &frozen_time));
    frozen_clock_.SetNow(frozen_time);
    manager_->SetClockForTesting(&frozen_clock_);
  }

  void SetWebRtcEventLoggingState(PeerConnectionKey key,
                                  bool event_logging_enabled) {
    webrtc_state_change_instructions_.emplace(key, event_logging_enabled);
  }

  void ExpectWebRtcStateChangeInstruction(int render_process_id,
                                          int lid,
                                          bool enabled) {
    ASSERT_FALSE(webrtc_state_change_instructions_.empty());
    auto& instruction = webrtc_state_change_instructions_.front();
    EXPECT_EQ(instruction.key, PeerConnectionKey(render_process_id, lid));
    EXPECT_EQ(instruction.enabled, enabled);
    webrtc_state_change_instructions_.pop();
  }

  void SetPeerConnectionTrackerProxyForTesting(
      std::unique_ptr<WebRtcEventLogManager::PeerConnectionTrackerProxy>
          pc_tracker_proxy) {
    manager_->SetPeerConnectionTrackerProxyForTesting(
        std::move(pc_tracker_proxy));
  }

  std::unique_ptr<TestBrowserContext> CreateBrowserContext(
      base::FilePath local_logs_base_path = base::FilePath()) {
    auto browser_context =
        std::make_unique<TestBrowserContext>(local_logs_base_path);

    // Blocks on the unit under test's task runner, so that we won't proceed
    // with the test (e.g. check that files were created) before finished
    // processing this even (which is signaled to it from
    //  BrowserContext::EnableForBrowserContext).
    WaitForPendingTasks();

    return browser_context;
  }

  base::FilePath GetLogsDirectoryPath(
      const base::FilePath& browser_context_dir) {
    return WebRtcRemoteEventLogManager::GetLogsDirectoryPath(
        browser_context_dir);
  }

  // Initiate an arbitrary synchronous operation, allowing any tasks pending
  // on the manager's internal task queue to be completed.
  // If given a RunLoop, we first block on it. The reason to do both is that
  // with the RunLoop we wait on some tasks which we know also post additional
  // tasks, then, after that chain is completed, we also wait for any potential
  // leftovers. For example, the run loop could wait for n-1 files to be
  // uploaded, then it is released when the last one's upload is initiated,
  // then we wait for the last file's upload to be completed.
  void WaitForPendingTasks(base::RunLoop* run_loop = nullptr) {
    if (run_loop) {
      run_loop->Run();
    }

    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    manager_->GetTaskRunnerForTesting()->PostTask(
        FROM_HERE,
        base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                       &event));
    event.Wait();
  }

  void SuppressUploading() {
    if (!upload_suppressing_browser_context_) {  // First suppression.
      upload_suppressing_browser_context_ = CreateBrowserContext();
    }
    DCHECK(!upload_suppressing_rph_) << "Uploading already suppressed.";
    upload_suppressing_rph_ = std::make_unique<MockRenderProcessHost>(
        upload_suppressing_browser_context_.get());
    ASSERT_TRUE(PeerConnectionAdded(upload_suppressing_rph_->GetID(), 0));
  }

  void UnsuppressUploading() {
    DCHECK(upload_suppressing_rph_) << "Uploading not suppressed.";
    ASSERT_TRUE(PeerConnectionRemoved(upload_suppressing_rph_->GetID(), 0));
    upload_suppressing_rph_.reset();
  }

  void ExpectFileContents(const base::FilePath& file_path,
                          const std::string& expected_contents) {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
    EXPECT_EQ(file_contents, expected_contents);
  }

  static const size_t kArbitraryVeryLargeFileSize = 1000 * 1000 * 1000;

  // Testing utilities.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  base::SimpleTestClock frozen_clock_;

  // The main loop, which allows waiting for the operations invoked on the
  // unit-under-test to be completed. Do not use this object directly from the
  // tests, since that would be error-prone. (Specifically, one must not produce
  // two events that could produce replies, without waiting on the first reply
  // in between.)
  std::unique_ptr<base::RunLoop> run_loop_;

  // Allows waiting for upload operations.
  std::unique_ptr<base::RunLoop> uploader_run_loop_;

  // Unit under test.
  std::unique_ptr<WebRtcEventLogManager> manager_;

  // Default BrowserContext and RenderProcessHost, to be used by tests which
  // do not require anything fancy (such as seeding the BrowserContext with
  // pre-existing logs files from a previous session, or working with multiple
  // BrowserContext objects).
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> rph_;

  // Used for suppressing the upload of finished files, by creating an active
  // remote-bound log associated with an independent BrowserContext which
  // does not otherwise interfere with the test.
  std::unique_ptr<TestBrowserContext> upload_suppressing_browser_context_;
  std::unique_ptr<MockRenderProcessHost> upload_suppressing_rph_;

  // The directory where we'll save local log files.
  base::FilePath local_logs_base_dir_;
  // local_logs_base_dir_ +  log files' name prefix.
  base::FilePath local_logs_base_path_;

  // WebRtcEventLogManager instructs WebRTC, via PeerConnectionTracker, to
  // only send WebRTC messages for certain peer connections. Some tests make
  // sure that this is done correctly, by waiting for these notifications, then
  // testing them.
  // Because a single action - disabling of local logging - could crease a
  // series of such instructions, we keep a queue of them. However, were one
  // to actually test that scenario, one would have to account for the lack
  // of a guarantee over the order in which these instructions are produced.
  struct WebRtcStateChangeInstruction {
    WebRtcStateChangeInstruction(PeerConnectionKey key, bool enabled)
        : key(key), enabled(enabled) {}
    PeerConnectionKey key;
    bool enabled;
  };
  std::queue<WebRtcStateChangeInstruction> webrtc_state_change_instructions_;
};

namespace {

// Common default/arbitrary values.
static constexpr int kPeerConnectionId = 478;

class MockWebRtcLocalEventLogsObserver : public WebRtcLocalEventLogsObserver {
 public:
  ~MockWebRtcLocalEventLogsObserver() override = default;
  MOCK_METHOD2(OnLocalLogStarted,
               void(PeerConnectionKey, const base::FilePath&));
  MOCK_METHOD1(OnLocalLogStopped, void(PeerConnectionKey));
};

class MockWebRtcRemoteEventLogsObserver : public WebRtcRemoteEventLogsObserver {
 public:
  ~MockWebRtcRemoteEventLogsObserver() override = default;
  MOCK_METHOD2(OnRemoteLogStarted,
               void(PeerConnectionKey, const base::FilePath&));
  MOCK_METHOD1(OnRemoteLogStopped, void(PeerConnectionKey));
};

class MockWebRtcEventLogUploaderObserver
    : public WebRtcEventLogUploaderObserver {
 public:
  ~MockWebRtcEventLogUploaderObserver() override = default;
  MOCK_METHOD2(OnWebRtcEventLogUploadComplete,
               void(const base::FilePath&, bool));
};

auto SaveFilePathTo(base::Optional<base::FilePath>* output) {
  return [output](PeerConnectionKey ignored_key, base::FilePath file_path) {
    *output = file_path;
  };
}

auto SaveKeyAndFilePathTo(base::Optional<PeerConnectionKey>* key_output,
                          base::Optional<base::FilePath>* file_path_output) {
  return [key_output, file_path_output](PeerConnectionKey key,
                                        base::FilePath file_path) {
    *key_output = key;
    *file_path_output = file_path;
  };
}

class PeerConnectionTrackerProxyForTesting
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  explicit PeerConnectionTrackerProxyForTesting(WebRtcEventLogManagerTest* test)
      : test_(test) {}

  ~PeerConnectionTrackerProxyForTesting() override = default;

  void SetWebRtcEventLoggingState(WebRtcEventLogPeerConnectionKey key,
                                  bool event_logging_enabled) override {
    test_->SetWebRtcEventLoggingState(key, event_logging_enabled);
  }

 private:
  WebRtcEventLogManagerTest* const test_;
};

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
void RemoveWritePermissionsFromDirectory(const base::FilePath& path) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  permissions &= ~write_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

// Production code uses the WebRtcEventLogUploaderObserver interface to
// pass notifications of an upload's completion from the concrete uploader
// to the remote-logs-manager. In unit tests, these notifications are
// intercepted, inspected and then passed onwards to the remote-logs-manager.
// This class simplifies this by getting a hold of the message, sending it
// to the unit test's observer, then allowing it to proceed to its normal
// destination.
class InterceptingWebRtcEventLogUploaderObserver
    : public WebRtcEventLogUploaderObserver {
 public:
  InterceptingWebRtcEventLogUploaderObserver(
      WebRtcEventLogUploaderObserver* intercpeting_observer,
      WebRtcEventLogUploaderObserver* normal_observer)
      : intercpeting_observer_(intercpeting_observer),
        normal_observer_(normal_observer) {}

  ~InterceptingWebRtcEventLogUploaderObserver() override = default;

  void OnWebRtcEventLogUploadComplete(const base::FilePath& file_path,
                                      bool upload_successful) override {
    intercpeting_observer_->OnWebRtcEventLogUploadComplete(file_path,
                                                           upload_successful);
    normal_observer_->OnWebRtcEventLogUploadComplete(file_path,
                                                     upload_successful);
  }

 private:
  WebRtcEventLogUploaderObserver* const intercpeting_observer_;
  WebRtcEventLogUploaderObserver* const normal_observer_;
};

// The factory for the following fake uploader produces a sequence of uploaders
// which fail the test if given a file other than that which they expect. The
// factory itself likewise fails the test if destroyed before producing all
// expected uploaders, or if it's asked for more uploaders than it expects to
// create. This allows us to test for sequences of uploads.
class FileListExpectingWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  // The logic is in the factory; the uploader just reports success so that the
  // next file may become eligible for uploading.
  explicit FileListExpectingWebRtcEventLogUploader(
      const base::FilePath& log_file,
      bool result,
      WebRtcEventLogUploaderObserver* observer) {
    observer->OnWebRtcEventLogUploadComplete(log_file, result);
  }

  ~FileListExpectingWebRtcEventLogUploader() override = default;

  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    Factory(std::list<base::FilePath>* expected_files,
            bool result,
            base::RunLoop* run_loop)
        : result_(result), run_loop_(run_loop) {
      expected_files_.swap(*expected_files);
    }

    ~Factory() override { EXPECT_TRUE(expected_files_.empty()); }

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const base::FilePath& log_file,
        WebRtcEventLogUploaderObserver* observer) override {
      if (expected_files_.empty()) {
        EXPECT_FALSE(true);  // More files uploaded than expected.
      } else {
        EXPECT_EQ(log_file, expected_files_.front());
        base::DeleteFile(log_file, false);
        expected_files_.pop_front();
      }

      if (expected_files_.empty()) {
        run_loop_->QuitWhenIdle();
      }

      return std::make_unique<FileListExpectingWebRtcEventLogUploader>(
          log_file, result_, observer);
    }

   private:
    std::list<base::FilePath> expected_files_;
    const bool result_;
    base::RunLoop* const run_loop_;
  };
};

}  // namespace

TEST_F(WebRtcEventLogManagerTest, LocalLogPeerConnectionAddedReturnsTrue) {
  EXPECT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogPeerConnectionAddedReturnsFalseIfAlreadyAdded) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_FALSE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogPeerConnectionRemovedReturnsTrue) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogPeerConnectionRemovedReturnsFalseIfNeverAdded) {
  EXPECT_FALSE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogPeerConnectionRemovedReturnsFalseIfAlreadyRemoved) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  EXPECT_FALSE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogEnableLocalLoggingReturnsTrue) {
  EXPECT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogEnableLocalLoggingReturnsFalseIfCalledWhenAlreadyEnabled) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_FALSE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, LocalLogDisableLocalLoggingReturnsTrue) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogDisableLocalLoggingReturnsIfNeverEnabled) {
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogDisableLocalLoggingReturnsIfAlreadyDisabled) {
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseWhenAllLoggingDisabled) {
  // Note that EnableLocalLogging() and StartRemoteLogging() weren't called.
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log"),
            std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseForUnknownPeerConnection) {
  ASSERT_TRUE(EnableLocalLogging());
  // Note that PeerConnectionAdded() wasn't called.
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log"),
            std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsLocalTrueWhenPcKnownAndLocalLoggingOn) {
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log"),
            std::make_pair(true, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsRemoteTrueWhenPcKnownAndRemoteLogging) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log"),
            std::make_pair(false, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsTrueAndTrueeWhenAllLoggingEnabled) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log"),
            std::make_pair(true, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedNotCalledIfLocalLoggingEnabledWithoutPeerConnections) {
  MockWebRtcLocalEventLogsObserver observer;
  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedNotCalledIfLocalLoggingDisabledWithoutPeerConnections) {
  MockWebRtcLocalEventLogsObserver observer;
  EXPECT_CALL(observer, OnLocalLogStopped(_)).Times(0);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForPeerConnectionAddedAndLocalLoggingEnabled) {
  MockWebRtcLocalEventLogsObserver observer;
  PeerConnectionKey peer_connection(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnLocalLogStarted(peer_connection, _)).Times(1);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForLocalLoggingEnabledAndPeerConnectionAdded) {
  MockWebRtcLocalEventLogsObserver observer;
  PeerConnectionKey peer_connection(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnLocalLogStarted(peer_connection, _)).Times(1);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterLocalLoggingDisabled) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  PeerConnectionKey peer_connection(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnLocalLogStopped(peer_connection)).Times(1);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterPeerConnectionRemoved) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  PeerConnectionKey peer_connection(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnLocalLogStopped(peer_connection)).Times(1);
  SetLocalLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, RemovedLocalLogsObserverReceivesNoCalls) {
  StrictMock<MockWebRtcLocalEventLogsObserver> observer;
  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(observer, OnLocalLogStopped(_)).Times(0);
  SetLocalLogsObserver(&observer);
  SetLocalLogsObserver(nullptr);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreatesEmptyFileWhenStarted) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreateAndWriteToFile) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log = "To strive, to seek, to find, and not to yield.";
  ASSERT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, log),
            std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleWritesToSameFile) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string logs[] = {"Old age hath yet his honour and his toil;",
                              "Death closes all: but something ere the end,",
                              "Some work of noble note, may yet be done,",
                              "Not unbecoming men that strove with Gods."};
  for (const std::string& log : logs) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
              std::make_pair(true, false));
  }

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFileSizeLimitNotExceeded) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "There lies the port; the vessel puffs her sail:";
  const size_t file_size_limit_bytes = log.length() / 2;

  ASSERT_TRUE(EnableLocalLogging(file_size_limit_bytes));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // A failure is reported, because not everything could be written. The file
  // will also be closed.
  const auto pc = PeerConnectionKey(key.render_process_id, key.lid);
  EXPECT_CALL(observer, OnLocalLogStopped(pc)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, false));

  // Additional calls to Write() have no effect.
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "ignored"),
            std::make_pair(false, false));

  ExpectFileContents(*file_path, log.substr(0, file_size_limit_bytes));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityOverUnlimitedFileSizes) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging(kWebRtcEventLogManagerUnlimitedFileSize));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log1 = "Who let the dogs out?";
  const std::string log2 = "Woof, woof, woof, woof, woof!";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log1),
            std::make_pair(true, false));
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log2),
            std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(*file_path, log1 + log2);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogNoWriteAfterLogStopped) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log_before = "log_before_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log_before),
            std::make_pair(true, false));
  EXPECT_CALL(observer, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  const std::string log_after = "log_after_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log_after),
            std::make_pair(false, false));

  ExpectFileContents(*file_path, log_before);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogOnlyWritesTheLogsAfterStarted) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  // Calls to Write() before the log was started are ignored.
  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  const std::string log1 = "The lights begin to twinkle from the rocks:";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log1),
            std::make_pair(false, false));
  ASSERT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_));

  base::Optional<base::FilePath> file_path;
  EXPECT_CALL(observer, OnLocalLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Calls after the log started have an effect. The calls to Write() from
  // before the log started are not remembered.
  const std::string log2 = "The long day wanes: the slow moon climbs: the deep";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log2),
            std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));

  ExpectFileContents(*file_path, log2);
}

// Note: This test also covers the scenario LocalLogExistingFilesNotOverwritten,
// which is therefore not explicitly tested.
TEST_F(WebRtcEventLogManagerTest, LocalLoggingRestartCreatesNewFile) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const std::vector<std::string> logs = {"<setup>", "<punchline>", "<encore>"};
  std::vector<base::Optional<PeerConnectionKey>> keys(logs.size());
  std::vector<base::Optional<base::FilePath>> file_paths(logs.size());

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));

  for (size_t i = 0; i < logs.size(); i++) {
    ON_CALL(observer, OnLocalLogStarted(_, _))
        .WillByDefault(Invoke(SaveKeyAndFilePathTo(&keys[i], &file_paths[i])));
    ASSERT_TRUE(EnableLocalLogging());
    ASSERT_TRUE(keys[i]);
    ASSERT_EQ(*keys[i], PeerConnectionKey(rph_->GetID(), kPeerConnectionId));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
    ASSERT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, logs[i]),
              std::make_pair(true, false));
    ASSERT_TRUE(DisableLocalLogging());
  }

  for (size_t i = 0; i < logs.size(); i++) {
    ExpectFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleActiveFiles) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  ASSERT_TRUE(EnableLocalLogging());

  std::list<MockRenderProcessHost> rphs;
  for (size_t i = 0; i < 3; i++) {
    rphs.emplace_back(browser_context_.get());
  };

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.emplace_back(rph.GetID(), kPeerConnectionId);
  }

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); i++) {
    ON_CALL(observer, OnLocalLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); i++) {
    logs.emplace_back(std::to_string(rph_->GetID()) +
                      std::to_string(kPeerConnectionId));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(true, false));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  ASSERT_TRUE(DisableLocalLogging());

  for (size_t i = 0; i < keys.size(); i++) {
    ExpectFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogLimitActiveLocalLogFiles) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; i++) {
    const PeerConnectionKey key(rph_->GetID(), i);
    EXPECT_CALL(observer, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), i));
  }

  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kMaxLocalLogFiles));
}

// When a log reaches its maximum size limit, it is closed, and no longer
// counted towards the limit.
TEST_F(WebRtcEventLogManagerTest, LocalLogFilledLogNotCountedTowardsLogsLimit) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const std::string log = "very_short_log";
  ASSERT_TRUE(EnableLocalLogging(log.size()));

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; i++) {
    const PeerConnectionKey key(rph_->GetID(), i);
    EXPECT_CALL(observer, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  // By writing to one of the logs, we fill it and end up closing it, allowing
  // an additional log to be written.
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), 0, log),
            std::make_pair(true, false));

  // We now have room for one additional log.
  const PeerConnectionKey last_key(rph_->GetID(), kMaxLocalLogFiles);
  EXPECT_CALL(observer, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; i++) {
    const PeerConnectionKey key(rph_->GetID(), i);
    EXPECT_CALL(observer, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  // When one peer connection is removed, one log is stopped, thereby allowing
  // an additional log to be opened.
  EXPECT_CALL(observer, OnLocalLogStopped(PeerConnectionKey(rph_->GetID(), 0)))
      .Times(1);
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), 0));

  // We now have room for one additional log.
  const PeerConnectionKey last_key(rph_->GetID(), kMaxLocalLogFiles);
  EXPECT_CALL(observer, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityDestructorWithActiveFile) {
  // We don't actually test that the file was closed, because this behavior
  // is not supported - WebRtcEventLogManager's destructor may never be called,
  // except for in unit tests.
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  DestroyUnitUnderTest();  // Explicitly show destruction does not crash.
}

TEST_F(WebRtcEventLogManagerTest, LocalLogIllegalPath) {
  StrictMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(observer, OnLocalLogStopped(_)).Times(0);

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  const base::FilePath illegal_path(FILE_PATH_LITERAL(":!@#$%|`^&*\\/"));
  EXPECT_TRUE(EnableLocalLogging(illegal_path));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_));
}

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
TEST_F(WebRtcEventLogManagerTest, LocalLogLegalPathWithoutPermissionsSanity) {
  RemoveWritePermissionsFromDirectory(local_logs_base_dir_);

  StrictMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(observer, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(observer, OnLocalLogStopped(_)).Times(0);

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  EXPECT_TRUE(EnableLocalLogging(local_logs_base_path_));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_));

  // Write() has no effect (but is handled gracefully).
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId,
                                  "Why did the chicken cross the road?"),
            std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_));

  // Logging was enabled, even if it had no effect because of the lacking
  // permissions; therefore, the operation of disabling it makes sense.
  EXPECT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_));
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

TEST_F(WebRtcEventLogManagerTest, LocalLogEmptyStringHandledGracefully) {
  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  base::Optional<base::FilePath> file_path;

  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); i++) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, logs[i]),
              std::make_pair(true, false));
  }
  ASSERT_TRUE(DisableLocalLogging());

  ExpectFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFilenameMatchesExpectedFormat) {
  using StringType = base::FilePath::StringType;

  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

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
  const base::FilePath local_logs_base_path =
      local_logs_base_dir_.Append(user_defined);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path = local_logs_base_path;
  expected_path = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + IntToStringType(kPeerConnectionId));
  expected_path = expected_path.AddExtension(FILE_PATH_LITERAL("log"));

  EXPECT_EQ(file_path, expected_path);
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogFilenameMatchesExpectedFormatRepeatedFilename) {
  using StringType = base::FilePath::StringType;

  NiceMock<MockWebRtcLocalEventLogsObserver> observer;
  SetLocalLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path_1;
  base::Optional<base::FilePath> file_path_2;
  EXPECT_CALL(observer, OnLocalLogStarted(key, _))
      .WillOnce(Invoke(SaveFilePathTo(&file_path_1)))
      .WillOnce(Invoke(SaveFilePathTo(&file_path_2)));

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

  const StringType user_defined_portion = FILE_PATH_LITERAL("user_defined");
  const base::FilePath local_logs_base_path =
      local_logs_base_dir_.Append(user_defined_portion);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path_1);
  ASSERT_FALSE(file_path_1->empty());

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path_1 = local_logs_base_path;
  expected_path_1 = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + IntToStringType(kPeerConnectionId));
  expected_path_1 = expected_path_1.AddExtension(FILE_PATH_LITERAL("log"));

  ASSERT_EQ(file_path_1, expected_path_1);

  ASSERT_TRUE(DisableLocalLogging());
  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(file_path_2);
  ASSERT_FALSE(file_path_2->empty());

  const base::FilePath expected_path_2 =
      expected_path_1.InsertBeforeExtension(FILE_PATH_LITERAL(" (1)"));

  // Focus of the test - starting the same log again produces a new file,
  // with an expected new filename.
  ASSERT_EQ(file_path_2, expected_path_2);
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStartedNotCalledIfRemoteLoggingNotEnabled) {
  MockWebRtcRemoteEventLogsObserver observer;
  EXPECT_CALL(observer, OnRemoteLogStarted(_, _)).Times(0);
  SetRemoteLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedNotCalledIfRemoteLoggingNotEnabled) {
  MockWebRtcRemoteEventLogsObserver observer;
  EXPECT_CALL(observer, OnRemoteLogStopped(_)).Times(0);
  SetRemoteLogsObserver(&observer);
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStartedCalledIfRemoteLoggingEnabled) {
  MockWebRtcRemoteEventLogsObserver observer;
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnRemoteLogStarted(key, _)).Times(1);
  SetRemoteLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedCalledIfRemoteLoggingEnabledThenPcRemoved) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnRemoteLogStopped(key)).Times(1);
  SetRemoteLogsObserver(&observer);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       BrowserContextInitializationCreatesDirectoryForRemoteLogs) {
  auto browser_context = CreateBrowserContext();
  const base::FilePath remote_logs_path =
      GetLogsDirectoryPath(browser_context->GetPath());
  EXPECT_TRUE(base::DirectoryExists(remote_logs_path));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnknownPeerConnection) {
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsTrueIfKnownPeerConnection) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfRestartAttempt) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnlimitedFileSize) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId,
                                  kWebRtcEventLogManagerUnlimitedFileSize));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfPeerConnectionAlreadyClosed) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, StartRemoteLoggingCreatesEmptyFile) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  base::Optional<base::FilePath> file_path;
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  SetRemoteLogsObserver(&observer);

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));

  ExpectFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileCreatedInCorrectDirectory) {
  // Set up separate browser contexts; each one will get one log.
  constexpr size_t kLogsNum = 3;
  std::unique_ptr<TestBrowserContext> browser_contexts[kLogsNum] = {
      CreateBrowserContext(), CreateBrowserContext(), CreateBrowserContext()};
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (auto& browser_context : browser_contexts) {
    rphs.emplace_back(
        std::make_unique<MockRenderProcessHost>(browser_context.get()));
  }

  // Prepare to store the logs' paths in distinct memory locations.
  base::Optional<base::FilePath> file_paths[kLogsNum];
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  for (size_t i = 0; i < kLogsNum; i++) {
    const PeerConnectionKey key(rphs[i]->GetID(), kPeerConnectionId);
    EXPECT_CALL(observer, OnRemoteLogStarted(key, _))
        .Times(1)
        .WillOnce(Invoke(SaveFilePathTo(&file_paths[i])));
  }
  SetRemoteLogsObserver(&observer);

  // Start one log for each browser context.
  for (const auto& rph : rphs) {
    ASSERT_TRUE(PeerConnectionAdded(rph->GetID(), kPeerConnectionId));
    ASSERT_TRUE(StartRemoteLogging(rph->GetID(), kPeerConnectionId));
  }

  // All log files must be created in their own context's directory.
  for (size_t i = 0; i < arraysize(browser_contexts); i++) {
    ASSERT_TRUE(file_paths[i]);
    EXPECT_TRUE(browser_contexts[i]->GetPath().IsParent(*file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteWritesToTheRemoteBoundFile) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  base::Optional<base::FilePath> file_path;
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  EXPECT_CALL(observer, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  SetRemoteLogsObserver(&observer);

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));

  const char* const log = "1 + 1 = 3";
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, log),
            std::make_pair(false, true));

  ExpectFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, WriteToBothLocalAndRemoteFiles) {
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  NiceMock<MockWebRtcLocalEventLogsObserver> local_observer;
  base::Optional<base::FilePath> local_path;
  EXPECT_CALL(local_observer, OnLocalLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&local_path)));
  SetLocalLogsObserver(&local_observer);

  NiceMock<MockWebRtcRemoteEventLogsObserver> remote_observer;
  base::Optional<base::FilePath> remote_path;
  EXPECT_CALL(remote_observer, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&remote_path)));
  SetRemoteLogsObserver(&remote_observer);

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));

  ASSERT_TRUE(local_path);
  ASSERT_FALSE(local_path->empty());
  ASSERT_TRUE(remote_path);
  ASSERT_FALSE(remote_path->empty());

  const char* const log = "logloglog";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(true, true));

  // Ensure the flushing of the file to disk before attempting to read them.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  for (base::Optional<base::FilePath> file_path : {local_path, remote_path}) {
    ExpectFileContents(*file_path, log);
  }
}

TEST_F(WebRtcEventLogManagerTest, MultipleWritesToSameRemoteBoundLogfile) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string logs[] = {"ABC", "DEF", "XYZ"};
  for (const std::string& log : logs) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
              std::make_pair(false, true));
  }

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileSizeLimitNotExceeded) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "tpyo";
  const size_t file_size_limit_bytes = log.length() / 2;

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid,
                                 file_size_limit_bytes));

  // A failure is reported, because not everything could be written. The file
  // will also be closed.
  EXPECT_CALL(observer, OnRemoteLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, false));

  // Additional calls to Write() have no effect.
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "ignored"),
            std::make_pair(false, false));

  ExpectFileContents(*file_path, log.substr(0, file_size_limit_bytes));
}

TEST_F(WebRtcEventLogManagerTest,
       LogMultipleActiveRemoteLogsSameBrowserContext) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const std::vector<PeerConnectionKey> keys = {
      PeerConnectionKey(rph_->GetID(), 0), PeerConnectionKey(rph_->GetID(), 1),
      PeerConnectionKey(rph_->GetID(), 2)};

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); i++) {
    ON_CALL(observer, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(StartRemoteLogging(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); i++) {
    logs.emplace_back(std::to_string(rph_->GetID()) + std::to_string(i));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  for (size_t i = 0; i < keys.size(); i++) {
    ExpectFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest,
       LogMultipleActiveRemoteLogsDifferentBrowserContexts) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  constexpr size_t kLogsNum = 3;
  std::unique_ptr<TestBrowserContext> browser_contexts[kLogsNum] = {
      CreateBrowserContext(), CreateBrowserContext(), CreateBrowserContext()};
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (auto& browser_context : browser_contexts) {
    rphs.emplace_back(
        std::make_unique<MockRenderProcessHost>(browser_context.get()));
  }

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.emplace_back(rph->GetID(), kPeerConnectionId);
  }

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); i++) {
    ON_CALL(observer, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(StartRemoteLogging(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); i++) {
    logs.emplace_back(std::to_string(rph_->GetID()) + std::to_string(i));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  for (size_t i = 0; i < keys.size(); i++) {
    ExpectFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, DifferentRemoteLogsMayHaveDifferentMaximums) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const std::vector<PeerConnectionKey> keys = {
      PeerConnectionKey(rph_->GetID(), 0), PeerConnectionKey(rph_->GetID(), 1)};
  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); i++) {
    ON_CALL(observer, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  const size_t file_size_limits_bytes[2] = {3, 5};
  CHECK_EQ(arraysize(file_size_limits_bytes), keys.size());

  const std::string log = "lorem ipsum";

  for (size_t i = 0; i < keys.size(); i++) {
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(StartRemoteLogging(keys[i].render_process_id, keys[i].lid,
                                   file_size_limits_bytes[i]));

    ASSERT_LT(file_size_limits_bytes[i], log.length());

    // A failure is reported, because not everything could be written. The file
    // will also be closed.
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, log),
        std::make_pair(false, false));
  }

  for (size_t i = 0; i < keys.size(); i++) {
    ExpectFileContents(*file_paths[i],
                       log.substr(0, file_size_limits_bytes[i]));
  }
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileClosedWhenCapacityReached) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);
  base::Optional<base::FilePath> file_path;
  ON_CALL(observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "Let X equal X.";

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid, log.length()));
  ASSERT_TRUE(file_path);

  EXPECT_CALL(observer, OnRemoteLogStopped(key)).Times(1);
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, true));
}

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
// TODO(eladalon): Add unit tests for lacking read permissions when looking
// to upload the file. https://crbug.com/775415
TEST_F(WebRtcEventLogManagerTest,
       FailureToCreateRemoteLogsDirHandledGracefully) {
  base::ScopedTempDir browser_context_dir;
  ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());
  RemoveWritePermissionsFromDirectory(browser_context_dir.GetPath());

  // Graceful handling by BrowserContext::EnableForBrowserContext.
  auto browser_context = CreateBrowserContext(browser_context_dir.Take());
  const base::FilePath remote_logs_path =
      GetLogsDirectoryPath(browser_context->GetPath());
  EXPECT_FALSE(base::DirectoryExists(remote_logs_path));

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());

  // Graceful handling of PeerConnectionAdded: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.)
  EXPECT_TRUE(PeerConnectionAdded(rph->GetID(), kPeerConnectionId));

  // Graceful handling of StartRemoteLogging: False returned because it's
  // impossible to write the log to a file.
  EXPECT_FALSE(StartRemoteLogging(rph->GetID(), kPeerConnectionId));

  // Graceful handling of OnWebRtcEventLogWrite: False returned because the log
  // could not be written at all, let alone in its entirety.
  const char* const log = "This is not a log.";
  EXPECT_EQ(OnWebRtcEventLogWrite(rph->GetID(), kPeerConnectionId, log),
            std::make_pair(false, false));

  // Graceful handling of PeerConnectionRemoved: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.
  EXPECT_TRUE(PeerConnectionRemoved(rph->GetID(), kPeerConnectionId));
}

TEST_F(WebRtcEventLogManagerTest, GracefullyHandleFailureToStartRemoteLogFile) {
  StrictMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);  // WebRTC logging will not be turned on.

  // Set up a BrowserContext whose directory we know, so that we would be
  // able to manipulate it.
  base::ScopedTempDir browser_context_dir;
  ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());
  auto browser_context = CreateBrowserContext(browser_context_dir.Take());
  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());

  // Remove write permissions from the directory.
  const base::FilePath remote_logs_path =
      GetLogsDirectoryPath(browser_context->GetPath());
  ASSERT_TRUE(base::DirectoryExists(remote_logs_path));
  RemoveWritePermissionsFromDirectory(remote_logs_path);

  // StartRemoteLogging() will now fail.
  const PeerConnectionKey key(rph->GetID(), kPeerConnectionId);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, key.lid));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "abc"),
            std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

TEST_F(WebRtcEventLogManagerTest, RemoteLogLimitActiveLogFiles) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  for (int i = 0; i < kMaxActiveRemoteLogFiles + 1; i++) {
    ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), i));
  }

  int i;
  for (i = 0; i < kMaxActiveRemoteLogFiles; i++) {
    EXPECT_CALL(observer,
                OnRemoteLogStarted(PeerConnectionKey(rph_->GetID(), i), _))
        .Times(1);
    ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), i));
  }

  EXPECT_CALL(observer, OnRemoteLogStarted(_, _)).Times(0);
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), i));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogFilledLogNotCountedTowardsLogsLimit) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const std::string log = "very_short_log";

  int i;
  for (i = 0; i < kMaxActiveRemoteLogFiles; i++) {
    ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), i));
    EXPECT_CALL(observer,
                OnRemoteLogStarted(PeerConnectionKey(rph_->GetID(), i), _))
        .Times(1);
    ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), i, log.length()));
  }

  // By writing to one of the logs until it reaches capacity, we fill it,
  // causing it to close, therefore allowing an additional log.
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), 0, log),
            std::make_pair(false, true));

  // We now have room for one additional log.
  ++i;
  EXPECT_CALL(observer,
              OnRemoteLogStarted(PeerConnectionKey(rph_->GetID(), i), _))
      .Times(1);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), i));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), i));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  for (int i = 0; i < kMaxActiveRemoteLogFiles; i++) {
    const PeerConnectionKey key(rph_->GetID(), i);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    EXPECT_CALL(observer, OnRemoteLogStarted(key, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  }

  // By removing a peer connection associated with one of the logs, we allow
  // an additional log.
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), 0));

  // We now have room for one additional log.
  const PeerConnectionKey last_key(rph_->GetID(), kMaxActiveRemoteLogFiles);
  EXPECT_CALL(observer, OnRemoteLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
  ASSERT_TRUE(StartRemoteLogging(last_key.render_process_id, last_key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       ActiveLogsForBrowserContextCountedTowardsItsPendingsLogsLimit) {
  SuppressUploading();

  // Produce kMaxPendingRemoteLogFiles pending logs.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; i++) {
    const PeerConnectionKey key(rph_->GetID(), i);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  // It is now impossible to start another *active* log for that BrowserContext,
  // because we have too many pending logs (and active logs become pending
  // once completed).
  const PeerConnectionKey forbidden(rph_->GetID(), kMaxPendingRemoteLogFiles);
  ASSERT_TRUE(PeerConnectionAdded(forbidden.render_process_id, forbidden.lid));
  EXPECT_FALSE(StartRemoteLogging(forbidden.render_process_id, forbidden.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       ObserveLimitOnMaximumPendingLogsPerBrowserContext) {
  // Keep one active peer connection on an independent BrowserContext in order
  // to suppress uploading of pending files during this test.
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), 1));

  // Create additional BrowserContexts for the test.
  std::unique_ptr<TestBrowserContext> browser_contexts[2] = {
      std::make_unique<TestBrowserContext>(),
      std::make_unique<TestBrowserContext>()};
  std::unique_ptr<MockRenderProcessHost> rphs[2] = {
      std::make_unique<MockRenderProcessHost>(browser_contexts[0].get()),
      std::make_unique<MockRenderProcessHost>(browser_contexts[1].get())};

  // Allowed to start kMaxPendingRemoteLogFiles for each BrowserContext.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; i++) {
    const PeerConnectionKey key(rphs[0]->GetID(), i);
    // The log could be opened:
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
    // The log changes state from ACTIVE to PENDING:
    EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  // Not allowed to start any more remote-bound logs for that BrowserContext.
  ASSERT_TRUE(PeerConnectionAdded(rphs[0]->GetID(), kMaxPendingRemoteLogFiles));
  EXPECT_FALSE(StartRemoteLogging(rphs[0]->GetID(), kMaxPendingRemoteLogFiles));

  // Other BrowserContexts aren't limit by the previous one's limit.
  ASSERT_TRUE(PeerConnectionAdded(rphs[1]->GetID(), 0));
  EXPECT_TRUE(StartRemoteLogging(rphs[1]->GetID(), 0));
}

TEST_F(WebRtcEventLogManagerTest,
       LogsFromPreviousSessionBecomePendingLogsWhenBrowserContextInitialized) {
  // Create a directory and seed it with log files, simulating the creation
  // of logs in a previous session.
  std::list<base::FilePath> expected_files;
  base::ScopedTempDir browser_context_dir;
  ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());

  const base::FilePath remote_logs_dir =
      GetLogsDirectoryPath(browser_context_dir.GetPath());
  ASSERT_TRUE(CreateDirectory(remote_logs_dir));

  for (size_t i = 0; i < kMaxPendingRemoteBoundWebRtcEventLogs; i++) {
    const base::FilePath file_path =
        remote_logs_dir.Append(IntToStringType(i))
            .AddExtension(kRemoteBoundLogExtension);
    constexpr int file_flags = base::File::FLAG_CREATE |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_EXCLUSIVE_WRITE;
    base::File file(file_path, file_flags);
    ASSERT_TRUE(file.IsValid() && file.created());
    expected_files.push_back(file_path);
  }

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  auto browser_context = CreateBrowserContext(browser_context_dir.Take());
  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTest, FinishedRemoteLogsUploadedAndFileDeleted) {
  // |upload_result| show that the files are deleted independent of the
  // upload's success / failure.
  const bool upload_result = GetParam();

  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<base::FilePath> expected_files = {*log_file};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, upload_result, &run_loop));

  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  const base::FilePath remote_logs_path =
      GetLogsDirectoryPath(browser_context_->GetPath());
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}

// Note that SuppressUploading() and UnSuppressUploading() use the behavior
// guaranteed by this test.
TEST_F(WebRtcEventLogManagerTest, UploadOnlyWhenNoActivePeerConnections) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey untracked(rph_->GetID(), 0);
  const PeerConnectionKey tracked(rph_->GetID(), 1);

  // Suppresses the uploading of the "tracked" peer connection's log.
  ASSERT_TRUE(PeerConnectionAdded(untracked.render_process_id, untracked.lid));

  // The tracked peer connection's log is not uploaded when finished, because
  // another peer connection is still active.
  base::Optional<base::FilePath> log_file;
  ON_CALL(observer, OnRemoteLogStarted(tracked, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(tracked.render_process_id, tracked.lid));
  ASSERT_TRUE(StartRemoteLogging(tracked.render_process_id, tracked.lid));
  ASSERT_TRUE(log_file);
  ASSERT_TRUE(PeerConnectionRemoved(tracked.render_process_id, tracked.lid));

  // Perform another action synchronously, so that we may be assured that the
  // observer's lack of callbacks was not a timing fluke.
  OnWebRtcEventLogWrite(untracked.render_process_id, untracked.lid, "Ook!");

  // Having been convinced that |tracked|'s log was not uploded while
  // |untracked| was active, close |untracked| and see that |tracked|'s log
  // is now uploaded.
  base::RunLoop run_loop;
  std::list<base::FilePath> expected_uploads = {*log_file};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_uploads, true, &run_loop));
  ASSERT_TRUE(
      PeerConnectionRemoved(untracked.render_process_id, untracked.lid));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest, UploadOrderDependsOnLastModificationTime) {
  SuppressUploading();

  // Create three directories and seed them with log files.
  base::FilePath browser_context_paths[3];
  base::FilePath file_paths[3];
  for (size_t i = 0; i < 3; i++) {
    base::ScopedTempDir browser_context_dir;
    ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());
    browser_context_paths[i] = browser_context_dir.Take();

    const base::FilePath remote_logs_dir =
        GetLogsDirectoryPath(browser_context_paths[i]);
    ASSERT_TRUE(CreateDirectory(remote_logs_dir));

    file_paths[i] = remote_logs_dir.AppendASCII("file").AddExtension(
        kRemoteBoundLogExtension);
    constexpr int file_flags = base::File::FLAG_CREATE |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_EXCLUSIVE_WRITE;
    base::File file(file_paths[i], file_flags);
    ASSERT_TRUE(file.IsValid() && file.created());
  }

  // Touch() requires setting the last access time as well. Keep it the same
  // for all files, showing that the difference between them lies elsewhere.
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_paths[0], &file_info));
  const base::Time shared_last_accessed = file_info.last_accessed;

  // Set the files' last modification time according to a non-trivial
  // permutation (not the order of creation or its reverse).
  const size_t permutation[3] = {2, 0, 1};
  std::list<base::FilePath> expected_files;
  base::Time mod_time = base::Time::Now() - base::TimeDelta::FromSeconds(3);
  for (size_t i = 0; i < 3; i++) {
    mod_time += base::TimeDelta::FromSeconds(1);  // Back to the future.
    const base::FilePath& path = file_paths[permutation[i]];
    ASSERT_TRUE(base::TouchFile(path, shared_last_accessed, mod_time));
    expected_files.emplace_back(path);
  }

  // Recognize the files as pending files by initializing their BrowserContexts.
  std::unique_ptr<TestBrowserContext> browser_contexts[3];
  std::unique_ptr<MockRenderProcessHost> rphs[3];
  for (size_t i = 0; i < 3; i++) {
    browser_contexts[i] = CreateBrowserContext(browser_context_paths[i]);
    rphs[i] =
        std::make_unique<MockRenderProcessHost>(browser_contexts[i].get());
  }

  // Show that the files are uploaded by order of modification.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  UnsuppressUploading();

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest, ExpiredFilesArePrunedRatherThanUploaded) {
  SuppressUploading();

  constexpr size_t kExpired = 0;
  constexpr size_t kFresh = 1;
  DCHECK_GE(kMaxPendingRemoteBoundWebRtcEventLogs, 2u)
      << "Please restructure the test to use separate browser contexts.";

  base::FilePath browser_context_path;
  base::FilePath file_paths[2];
  base::ScopedTempDir browser_context_dir;
  ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());
  browser_context_path = browser_context_dir.Take();

  const base::FilePath remote_logs_dir =
      GetLogsDirectoryPath(browser_context_path);
  ASSERT_TRUE(CreateDirectory(remote_logs_dir));

  for (size_t i = 0; i < 2; i++) {
    file_paths[i] = remote_logs_dir.Append(IntToStringType(i))
                        .AddExtension(kRemoteBoundLogExtension);
    constexpr int file_flags = base::File::FLAG_CREATE |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_EXCLUSIVE_WRITE;
    base::File file(file_paths[i], file_flags);
    ASSERT_TRUE(file.IsValid() && file.created());
  }

  // Touch() requires setting the last access time as well. Keep it current,
  // showing that only the last modification time matters.
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_paths[0], &file_info));

  // Set the expired file's last modification time to past max retention.
  const base::Time expired_mod_time = base::Time::Now() -
                                      kRemoteBoundWebRtcEventLogsMaxRetention -
                                      base::TimeDelta::FromSeconds(1);
  ASSERT_TRUE(base::TouchFile(file_paths[kExpired], file_info.last_accessed,
                              expired_mod_time));

  // Recognize the file as pending by initializing its BrowserContext.
  std::unique_ptr<TestBrowserContext> browser_context;
  browser_context = CreateBrowserContext(browser_context_path);

  // Show that the expired file is not uploaded.
  base::RunLoop run_loop;
  std::list<base::FilePath> expected_files = {file_paths[kFresh]};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  UnsuppressUploading();

  WaitForPendingTasks(&run_loop);

  // Both the uploaded file as well as the expired file have no been removed
  // from local disk.
  EXPECT_TRUE(
      base::IsDirectoryEmpty(GetLogsDirectoryPath(browser_context->GetPath())));
}

// TODO(eladalon): Add a test showing that a file expiring while another
// is being uploaded, is not uploaded after the current upload is completed.
// This is significant because Chrome might stay up for a long time.
// https://crbug.com/775415

TEST_F(WebRtcEventLogManagerTest, RemoteLogSanityDestructorWithActiveFile) {
  // We don't actually test that the file was closed, because this behavior
  // is not supported - WebRtcEventLogManager's destructor may never be called,
  // except for in unit tests.
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  DestroyUnitUnderTest();  // Explicitly show destruction does not crash.
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogEmptyStringHandledGracefully) {
  NiceMock<MockWebRtcRemoteEventLogsObserver> observer;
  SetRemoteLogsObserver(&observer);

  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  base::Optional<base::FilePath> file_path;

  ON_CALL(observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); i++) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, logs[i]),
              std::make_pair(false, true));
  }
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
TEST_F(WebRtcEventLogManagerTest,
       UnopenedRemoteLogFilesNotCountedTowardsActiveLogsLimit) {
  // Set up BrowserContexts whose directories we know, so that we would be
  // able to manipulate them.
  std::unique_ptr<TestBrowserContext> browser_contexts[2];
  std::unique_ptr<MockRenderProcessHost> rphs[2];
  for (size_t i = 0; i < 2; i++) {
    base::ScopedTempDir browser_context_dir;
    ASSERT_TRUE(browser_context_dir.CreateUniqueTempDir());
    browser_contexts[i] = CreateBrowserContext(browser_context_dir.Take());
    rphs[i] =
        std::make_unique<MockRenderProcessHost>(browser_contexts[i].get());
  }

  constexpr size_t without_permissions = 0;
  constexpr size_t with_permissions = 1;

  // Remove write permissions from one directory.
  const base::FilePath permissions_lacking_remote_logs_path =
      GetLogsDirectoryPath(browser_contexts[without_permissions]->GetPath());
  ASSERT_TRUE(base::DirectoryExists(permissions_lacking_remote_logs_path));
  RemoveWritePermissionsFromDirectory(permissions_lacking_remote_logs_path);

  // Fail to start a log associated with the permission-lacking directory.
  ASSERT_TRUE(PeerConnectionAdded(rphs[without_permissions]->GetID(), 0));
  ASSERT_FALSE(StartRemoteLogging(rphs[without_permissions]->GetID(), 0));

  // Show that this was not counted towards the limit of active files.
  for (int i = 0; i < kMaxActiveRemoteLogFiles; i++) {
    ASSERT_TRUE(PeerConnectionAdded(rphs[with_permissions]->GetID(), i));
    EXPECT_TRUE(StartRemoteLogging(rphs[with_permissions]->GetID(), i));
  }
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenLocalEnabledWithoutPeerConnection) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenPeerConnectionButNoLoggingEnabled) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenLocalEnabledThenPeerConnectionAdded) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenPeerConnectionAddedThenLocalEnabled) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenRemoteLoggingEnabled) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);
}

TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenLocalLoggingStopped) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, false);
}

// #1 - Local logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenPeerConnectionRemoved1) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, false);
}

// #2 - Remote logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenPeerConnectionRemoved2) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, false);
}

// #1 - Local logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging1) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// #2 - Remote logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging2) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingWhenRemoteLoggingEnabledDoesNotStopWebRtcLogging) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());

  // Cleanup
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, false);
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingAfterPcRemovalHasNoEffectOnWebRtcLogging) {
  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  ExpectWebRtcStateChangeInstruction(rph_->GetID(), kPeerConnectionId, false);
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// Once a peer connection with a given key was removed, it may not again be
// added. But, if this impossible case occurs, WebRtcEventLogManager will
// not crash.
TEST_F(WebRtcEventLogManagerTest, SanityOverRecreatingTheSamePeerConnection) {
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), kPeerConnectionId));
  OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log1");
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), kPeerConnectionId));
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kPeerConnectionId));
  OnWebRtcEventLogWrite(rph_->GetID(), kPeerConnectionId, "log2");
}

// The logs would typically be binary. However, the other tests only cover ASCII
// characters, for readability. This test shows that this is not a problem.
TEST_F(WebRtcEventLogManagerTest, LogAllPossibleCharacters) {
  const PeerConnectionKey key(rph_->GetID(), kPeerConnectionId);

  NiceMock<MockWebRtcLocalEventLogsObserver> local_observer;
  SetLocalLogsObserver(&local_observer);
  base::Optional<base::FilePath> local_log_file_path;
  ON_CALL(local_observer, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&local_log_file_path)));

  NiceMock<MockWebRtcRemoteEventLogsObserver> remote_observer;
  SetRemoteLogsObserver(&remote_observer);
  base::Optional<base::FilePath> remote_log_file_path;
  ON_CALL(remote_observer, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&remote_log_file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, key.lid));
  ASSERT_TRUE(local_log_file_path);
  ASSERT_FALSE(local_log_file_path->empty());
  ASSERT_TRUE(remote_log_file_path);
  ASSERT_FALSE(remote_log_file_path->empty());

  std::string all_chars;
  for (size_t i = 0; i < 256; i++) {
    all_chars += static_cast<uint8_t>(i);
  }
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, all_chars),
            std::make_pair(true, true));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectFileContents(*local_log_file_path, all_chars);
  ExpectFileContents(*remote_log_file_path, all_chars);
}

INSTANTIATE_TEST_CASE_P(UploadCompleteResult,
                        WebRtcEventLogManagerTest,
                        ::testing::Values(false, true));

}  // namespace content
