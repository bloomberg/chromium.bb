// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/mock_network_connection_tracker.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/775415): Add unit tests for incognito mode.
// TODO(crbug.com/775415): Migrate to being based on Profiles rather than on
// BrowserContexts.

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using BrowserContext = content::BrowserContext;
using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;
using MockRenderProcessHost = content::MockRenderProcessHost;
using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;
using RenderProcessHost = content::RenderProcessHost;

namespace {

#if !defined(OS_ANDROID)

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

const int kMaxActiveRemoteLogFiles =
    static_cast<int>(kMaxActiveRemoteBoundWebRtcEventLogs);
const int kMaxPendingRemoteLogFiles =
    static_cast<int>(kMaxPendingRemoteBoundWebRtcEventLogs);

base::Time GetLastModificationTime(const base::FilePath& file_path) {
  base::File::Info file_info;
  if (!base::GetFileInfo(file_path, &file_info)) {
    EXPECT_TRUE(false);
    return base::Time();
  }
  return file_info.last_modified;
}

#endif

// Common default/arbitrary values.
static constexpr int kLid = 478;

PeerConnectionKey GetPeerConnectionKey(const RenderProcessHost* rph, int lid) {
  const BrowserContext* browser_context = rph->GetBrowserContext();
  const auto browser_context_id = GetBrowserContextId(browser_context);
  return PeerConnectionKey(rph->GetID(), lid, browser_context_id);
}

// This implementation does not upload files, nor pretends to have finished an
// upload. Most importantly, it does not get rid of the locally-stored log file
// after finishing a simulated upload; this is useful because it keeps the file
// on disk, where unit tests may inspect it.
// This class enforces an expectation over the upload being cancelled or not.
class NullWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  NullWebRtcEventLogUploader(const WebRtcLogFileInfo& log_file,
                             bool cancellation_expected)
      : log_file_(log_file),
        cancellation_expected_(cancellation_expected),
        was_cancelled_(false) {}

  ~NullWebRtcEventLogUploader() override {
    EXPECT_EQ(was_cancelled_, cancellation_expected_);
  }

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override {
    return log_file_;
  }

  bool Cancel() override {
    EXPECT_TRUE(cancellation_expected_);
    if (was_cancelled_) {  // Should not be called more than once.
      EXPECT_TRUE(false);
      return false;
    }
    was_cancelled_ = true;
    return true;
  }

  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    explicit Factory(bool cancellation_expected)
        : cancellation_expected_(cancellation_expected) {}

    ~Factory() override = default;

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) override {
      return std::make_unique<NullWebRtcEventLogUploader>(
          log_file, cancellation_expected_);
    }

   private:
    const bool cancellation_expected_;
  };

 private:
  const WebRtcLogFileInfo log_file_;
  const bool cancellation_expected_;
  bool was_cancelled_;
};

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

}  // namespace

class WebRtcEventLogManagerTestBase : public ::testing::Test {
 public:
  WebRtcEventLogManagerTestBase()
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        run_loop_(std::make_unique<base::RunLoop>()),
        uploader_run_loop_(std::make_unique<base::RunLoop>()),
        browser_context_(nullptr),
        browser_context_id_(GetBrowserContextId(browser_context_)),
        upload_suppressing_browser_context_(nullptr) {
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        url_request_context_getter_.get());

    // Avoid proactive pruning; it has the potential to mess up tests, as well
    // as keep pendings tasks around with a dangling reference to the unit
    // under test. (Zero is a sentinel value for disabling proactive pruning.)
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogProactivePruningDelta, "0");

    EXPECT_TRUE(local_logs_base_dir_.CreateUniqueTempDir());
    local_logs_base_path_ = local_logs_base_dir_.GetPath().Append(
        FILE_PATH_LITERAL("local_event_logs"));

    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
  }

  ~WebRtcEventLogManagerTestBase() override {
    WaitForPendingTasks();

    // We do not want to satisfy any unsatisfied expectations by destroying
    // |rph_|, |browser_context_|, etc., at the end of the test, before we
    // destroy |event_log_manager_|. However, we must also make sure that their
    // destructors do not attempt to access |event_log_manager_|, which in
    // normal code lives forever, but not in the unit tests.
    event_log_manager_.reset();

    // Guard against unexpected state changes.
    EXPECT_TRUE(webrtc_state_change_instructions_.empty());
  }

  void SetUp() override {
    SetUp(std::make_unique<content::MockNetworkConnectionTracker>(
        true, network::mojom::ConnectionType::CONNECTION_ETHERNET));
  }

  void SetUp(std::unique_ptr<content::NetworkConnectionTracker> tracker) {
    TestingBrowserProcess::GetGlobal()->SetNetworkConnectionTracker(
        std::move(tracker));
    SetLocalLogsObserver(&local_observer_);
    SetRemoteLogsObserver(&remote_observer_);
    LoadProfiles();
  }

  void LoadProfiles() {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp(profiles_dir_.GetPath()));
    browser_context_ = CreateBrowserContext("browser_context_");
    browser_context_id_ = GetBrowserContextId(browser_context_);
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_);
  }

  void UnloadProfiles() {
    browser_context_ = nullptr;
    browser_context_id_ = GetBrowserContextId(browser_context_);
    rph_.reset();
    EXPECT_FALSE(upload_suppressing_rph_);
    testing_profile_manager_.reset();  // Make sure we only have one at a time.
  }

  void WaitForReply() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop);  // Allow re-blocking.
  }

  void Reply() { run_loop_->QuitWhenIdle(); }

  base::OnceClosure ReplyClosure() {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)() =
        &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this));
  }

  template <typename T>
  void Reply(T* output, T val) {
    *output = val;
    run_loop_->QuitWhenIdle();
  }

  template <typename T>
  base::OnceCallback<void(T)> ReplyClosure(T* output) {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)(T*, T) =
        &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this), output);
  }

  void Reply(bool* output_bool,
             std::string* output_str1,
             std::string* output_str2,
             bool bool_val,
             const std::string& str1_val,
             const std::string& str2_val) {
    *output_bool = bool_val;
    *output_str1 = str1_val;
    *output_str2 = str2_val;
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  ReplyClosure(bool* output_bool,
               std::string* output_str1,
               std::string* output_str2) {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)(
        bool*, std::string*, std::string*, bool, const std::string&,
        const std::string&) = &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this), output_bool,
                          output_str1, output_str2);
  }

  bool PeerConnectionAdded(int render_process_id,
                           int lid,
                           std::string peer_connection_id = std::string()) {
    if (peer_connection_id.empty()) {
      // If the test does not specify an explicit peer connection ID, then that
      // is not the focus of the test, and any unique identifier would do.
      peer_connection_id = GetUniqueId(render_process_id, lid);
    }

    bool result;
    event_log_manager_->PeerConnectionAdded(
        render_process_id, lid, peer_connection_id, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool PeerConnectionRemoved(int render_process_id, int lid) {
    bool result;
    event_log_manager_->PeerConnectionRemoved(render_process_id, lid,
                                              ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool PeerConnectionStopped(int render_process_id, int lid) {
    bool result;
    event_log_manager_->PeerConnectionStopped(render_process_id, lid,
                                              ReplyClosure(&result));
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
    event_log_manager_->EnableLocalLogging(local_logs_base_path, max_size_bytes,
                                           ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool DisableLocalLogging() {
    bool result;
    event_log_manager_->DisableLocalLogging(ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool StartRemoteLogging(int render_process_id,
                          const std::string& peer_connection_id,
                          size_t max_size_bytes,
                          std::string* log_id_output = nullptr,
                          std::string* error_message_output = nullptr) {
    bool result;
    std::string log_id;
    std::string error_message;

    event_log_manager_->StartRemoteLogging(
        render_process_id, peer_connection_id, max_size_bytes,
        ReplyClosure(&result, &log_id, &error_message));

    WaitForReply();

    // If successful, only |log_id|. If unsuccessful, only |error_message| set.
    DCHECK_EQ(result, !log_id.empty());
    DCHECK_EQ(!result, !error_message.empty());

    if (error_message_output) {
      *error_message_output = error_message;
    }

    if (log_id_output) {
      *log_id_output = log_id;
    }

    return result;
  }

  bool StartRemoteLogging(int render_process_id,
                          const std::string& peer_connection_id,
                          std::string* log_id_output = nullptr,
                          std::string* error_message_output = nullptr) {
    return StartRemoteLogging(render_process_id, peer_connection_id,
                              kMaxRemoteLogFileSizeBytes, log_id_output,
                              error_message_output);
  }

  void ClearCacheForBrowserContext(
      const content::BrowserContext* browser_context,
      const base::Time& delete_begin,
      const base::Time& delete_end) {
    event_log_manager_->ClearCacheForBrowserContext(
        browser_context, delete_begin, delete_end, ReplyClosure());
    WaitForReply();
  }

  void SetLocalLogsObserver(WebRtcLocalEventLogsObserver* observer) {
    event_log_manager_->SetLocalLogsObserver(observer, ReplyClosure());
    WaitForReply();
  }

  void SetRemoteLogsObserver(WebRtcRemoteEventLogsObserver* observer) {
    event_log_manager_->SetRemoteLogsObserver(observer, ReplyClosure());
    WaitForReply();
  }

  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> factory) {
    event_log_manager_->SetWebRtcEventLogUploaderFactoryForTesting(
        std::move(factory), ReplyClosure());
    WaitForReply();
  }

  std::pair<bool, bool> OnWebRtcEventLogWrite(int render_process_id,
                                              int lid,
                                              const std::string& message) {
    std::pair<bool, bool> result;
    event_log_manager_->OnWebRtcEventLogWrite(render_process_id, lid, message,
                                              ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  void FreezeClockAt(const base::Time::Exploded& frozen_time_exploded) {
    base::Time frozen_time;
    ASSERT_TRUE(
        base::Time::FromLocalExploded(frozen_time_exploded, &frozen_time));
    frozen_clock_.SetNow(frozen_time);
    event_log_manager_->SetClockForTesting(&frozen_clock_, ReplyClosure());
    WaitForReply();
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
    EXPECT_EQ(instruction.key.render_process_id, render_process_id);
    EXPECT_EQ(instruction.key.lid, lid);
    EXPECT_EQ(instruction.enabled, enabled);
    webrtc_state_change_instructions_.pop();
  }

  void SetPeerConnectionTrackerProxyForTesting(
      std::unique_ptr<WebRtcEventLogManager::PeerConnectionTrackerProxy>
          pc_tracker_proxy) {
    event_log_manager_->SetPeerConnectionTrackerProxyForTesting(
        std::move(pc_tracker_proxy), ReplyClosure());
    WaitForReply();
  }

  // |testing_profile_manager_| maintains ownership of the created objects.
  // This allows either creating a TestingProfile with a predetermined name
  // (useful when trying to "reload" a profile), or one with an arbitrary name.
  TestingProfile* CreateBrowserContext(const std::string& profile_name = "") {
    static size_t index = 0;
    TestingProfile* browser_context;
    if (profile_name.empty()) {
      browser_context = testing_profile_manager_->CreateTestingProfile(
          std::to_string(++index));
    } else {
      browser_context =
          testing_profile_manager_->CreateTestingProfile(profile_name);
    }

    // Blocks on the unit under test's task runner, so that we won't proceed
    // with the test (e.g. check that files were created) before finished
    // processing this even (which is signaled to it from
    //  BrowserContext::EnableForBrowserContext).
    WaitForPendingTasks();

    return browser_context;
  }

  base::FilePath RemoteBoundLogsDir(
      const BrowserContext* browser_context) const {
    return GetRemoteBoundWebRtcEventLogsDir(browser_context->GetPath());
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
    event_log_manager_->GetTaskRunnerForTesting()->PostTask(
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
        upload_suppressing_browser_context_);
    const auto key = GetPeerConnectionKey(upload_suppressing_rph_.get(), 0);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  void UnsuppressUploading() {
    DCHECK(upload_suppressing_rph_) << "Uploading not suppressed.";
    ASSERT_TRUE(PeerConnectionRemoved(upload_suppressing_rph_->GetID(), 0));
    upload_suppressing_rph_.reset();
  }

  void ExpectLocalFileContents(const base::FilePath& file_path,
                               const std::string& expected_contents) {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
    EXPECT_EQ(file_contents, expected_contents);
  }

  void ExpectRemoteFileContents(const base::FilePath& file_path,
                                const std::string& expected_event_log) {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
    EXPECT_EQ(file_contents, expected_event_log);
  }

  // When the peer connection's ID is not the focus of the test, this allows
  // us to conveniently assign unique IDs to peer connections.
  std::string GetUniqueId(int render_process_id, int lid) {
    return std::to_string(render_process_id) + "_" + std::to_string(lid);
  }
  std::string GetUniqueId(const PeerConnectionKey& key) {
    return GetUniqueId(key.render_process_id, key.lid);
  }

  bool UploadConditionsHold() {
    base::RunLoop run_loop;
    bool result;

    auto callback = [](base::RunLoop* run_loop, bool* result_out, bool result) {
      *result_out = result;
      run_loop->QuitWhenIdle();
    };

    event_log_manager_->UploadConditionsHoldForTesting(
        base::BindOnce(callback, &run_loop, &result));
    run_loop.Run();

    return result;
  }

  // Testing utilities.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  base::SimpleTestClock frozen_clock_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;

  // The main loop, which allows waiting for the operations invoked on the
  // unit-under-test to be completed. Do not use this object directly from the
  // tests, since that would be error-prone. (Specifically, one must not produce
  // two events that could produce replies, without waiting on the first reply
  // in between.)
  std::unique_ptr<base::RunLoop> run_loop_;

  // Allows waiting for upload operations.
  std::unique_ptr<base::RunLoop> uploader_run_loop_;

  // Unit under test.
  std::unique_ptr<WebRtcEventLogManager> event_log_manager_;

  // The directory which will contain all profiles.
  base::ScopedTempDir profiles_dir_;

  // Constructs (and owns) profiles.
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;

  // Default BrowserContext and RenderProcessHost, to be used by tests which
  // do not require anything fancy (such as seeding the BrowserContext with
  // pre-existing logs files from a previous session, or working with multiple
  // BrowserContext objects).

  TestingProfile* browser_context_;  // Owned by testing_profile_manager_.
  BrowserContextId browser_context_id_;
  std::unique_ptr<MockRenderProcessHost> rph_;

  // Used for suppressing the upload of finished files, by creating an active
  // remote-bound log associated with an independent BrowserContext which
  // does not otherwise interfere with the test.
  // upload_suppressing_browser_context_ is owned by testing_profile_manager_.
  TestingProfile* upload_suppressing_browser_context_;
  std::unique_ptr<MockRenderProcessHost> upload_suppressing_rph_;

  // The directory where we'll save local log files.
  base::ScopedTempDir local_logs_base_dir_;
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

  // Observers for local/remote logging being started/stopped. By having them
  // here, we achieve two goals:
  // 1. Reduce boilerplate in the tests themselves.
  // 2. Avoid lifetime issues, where the observer might be deallocated before
  //    a RenderProcessHost is deallocated (which can potentially trigger a
  //    callback on the observer).
  NiceMock<MockWebRtcLocalEventLogsObserver> local_observer_;
  NiceMock<MockWebRtcRemoteEventLogsObserver> remote_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogManagerTestBase);
};

#if !defined(OS_ANDROID)

class WebRtcEventLogManagerTest : public WebRtcEventLogManagerTestBase,
                                  public ::testing::WithParamInterface<bool> {
 public:
  WebRtcEventLogManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");

    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();
  }

  void SetUp() override {
    auto tracker = std::make_unique<content::MockNetworkConnectionTracker>(
        true, network::mojom::ConnectionType::CONNECTION_ETHERNET);
    WebRtcEventLogManagerTestBase::SetUp(std::move(tracker));
    SetWebRtcEventLogUploaderFactoryForTesting(
        std::make_unique<NullWebRtcEventLogUploader::Factory>(false));
  }
};

class WebRtcEventLogManagerTestCacheClearing
    : public WebRtcEventLogManagerTest {
 public:
  ~WebRtcEventLogManagerTestCacheClearing() override = default;

  void CreatePendingLogFiles(BrowserContext* browser_context) {
    ASSERT_TRUE(pending_logs_.find(browser_context) == pending_logs_.end());
    auto& elements = pending_logs_[browser_context];
    elements = std::make_unique<BrowserContextAssociatedElements>();

    for (size_t i = 0; i < kMaxActiveRemoteBoundWebRtcEventLogs; ++i) {
      elements->rphs.push_back(
          std::make_unique<MockRenderProcessHost>(browser_context));
      const auto key = GetPeerConnectionKey(elements->rphs[i].get(), kLid);
      elements->file_paths.push_back(CreatePendingRemoteLogFile(key));
      ASSERT_TRUE(elements->file_paths[i]);
      ASSERT_TRUE(base::PathExists(*elements->file_paths[i]));

      pending_latest_mod_ = GetLastModificationTime(*elements->file_paths[i]);
      if (pending_earliest_mod_.is_null()) {  // First file.
        pending_earliest_mod_ = pending_latest_mod_;
      }
    }
  }

  base::Optional<base::FilePath> CreateRemoteLogFile(
      const PeerConnectionKey& key,
      bool pending) {
    base::Optional<base::FilePath> file_path;
    ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
    EXPECT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    EXPECT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
    if (pending) {
      // Transition from ACTIVE to PENDING.
      EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
    }
    return file_path;
  }

  base::Optional<base::FilePath> CreateActiveRemoteLogFile(
      const PeerConnectionKey& key) {
    return CreateRemoteLogFile(key, false);
  }

  base::Optional<base::FilePath> CreatePendingRemoteLogFile(
      const PeerConnectionKey& key) {
    return CreateRemoteLogFile(key, true);
  }

 protected:
  // When closing a file, rather than check its last modification date, which
  // is potentially expensive, WebRtcRemoteEventLogManager reads the system
  // clock, which should be close enough. For tests, however, the difference
  // could be enough to flake the tests, if not for this epsilon. Given the
  // focus of the tests that use this, this epsilon can be arbitrarily large.
  static const base::TimeDelta kEpsion;

  struct BrowserContextAssociatedElements {
    std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
    std::vector<base::Optional<base::FilePath>> file_paths;
  };

  std::map<const BrowserContext*,
           std::unique_ptr<BrowserContextAssociatedElements>>
      pending_logs_;

  // Latest modification times of earliest and latest pending log files.
  base::Time pending_earliest_mod_;
  base::Time pending_latest_mod_;
};

const base::TimeDelta WebRtcEventLogManagerTestCacheClearing::kEpsion =
    base::TimeDelta::FromHours(1);

class WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled
    : public WebRtcEventLogManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled() {
    // Show that the feature is not active if not explicitly ENABLED.
    const bool disable = GetParam();
    if (disable) {  // Otherwise, left to default value.
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebRtcRemoteEventLog);
    }
    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();
  }

  ~WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled() override =
      default;
};

class WebRtcEventLogManagerTestUploadSuppressionDisablingFlag
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestUploadSuppressionDisablingFlag() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ::switches::kWebRtcRemoteEventLogUploadNoSuppression);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");

    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();
  }

  ~WebRtcEventLogManagerTestUploadSuppressionDisablingFlag() override = default;
};

class WebRtcEventLogManagerTestForNetworkConnectivity
    : public WebRtcEventLogManagerTestBase,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     network::mojom::ConnectionType,
                     network::mojom::ConnectionType>> {
 public:
  WebRtcEventLogManagerTestForNetworkConnectivity()
      : get_conn_type_is_sync_(std::get<0>(GetParam())),
        supported_type_(std::get<1>(GetParam())),
        unsupported_type_(std::get<2>(GetParam())) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");

    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();
  }

  ~WebRtcEventLogManagerTestForNetworkConnectivity() override = default;

  void SetUp() override {
    // Do nothing; the test body itself will call the super-class's SetUp
    // with the correct MockNetworkConnectionTracker.
  }

  void UnloadProfileAndSeedPendingLog() {
    DCHECK(browser_context_path_.empty()) << "Not expected to be called twice.";

    // Unload the profile, but remember where it stores its files (for sanity).
    browser_context_path_ = browser_context_->GetPath();
    const base::FilePath remote_logs_dir = RemoteBoundLogsDir(browser_context_);
    UnloadProfiles();

    // Seed the remote logs' directory with one log file, simulating the
    // creation of logs in a previous session.
    ASSERT_TRUE(CreateDirectory(remote_logs_dir));

    const base::FilePath file_path =
        remote_logs_dir.Append(kRemoteBoundWebRtcEventLogFileNamePrefix)
            .InsertBeforeExtensionASCII("01234567890123456789012345678901")
            .AddExtension(kRemoteBoundWebRtcEventLogExtension);
    constexpr int file_flags = base::File::FLAG_CREATE |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_EXCLUSIVE_WRITE;
    file_ = base::File(file_path, file_flags);
    ASSERT_TRUE(file_.IsValid() && file_.created());
    expected_files_.emplace_back(browser_context_id_, file_path,
                                 GetLastModificationTime(file_path));
  }

  const bool get_conn_type_is_sync_;
  const network::mojom::ConnectionType supported_type_;
  const network::mojom::ConnectionType unsupported_type_;

  base::FilePath browser_context_path_;  // For sanity over the test itself.
  std::list<WebRtcLogFileInfo> expected_files_;
  base::File file_;
};

class WebRtcEventLogManagerTestUploadDelay
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestUploadDelay() {}

  void SetUp() override {
    // Intercept and block the call to SetUp(). The test body will call
    // the version that sets an upload delay instead.
  }

  void SetUp(size_t upload_delay_ms) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs,
        std::to_string(upload_delay_ms));

    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();

    WebRtcEventLogManagerTestBase::SetUp();
  }

  ~WebRtcEventLogManagerTestUploadDelay() override = default;

  // There's a trade-off between the test runtime and the likelihood of a
  // false-positive (lowered when the time is increased).
  // Since false-positives can be caught handled even if only manifesting
  // occasionally, this value should be enough.
  static const size_t kDefaultUploadDelayMs = 500;

  // For tests where we don't intend to wait, prevent flakiness by setting
  // an unrealistically long delay.
  static const size_t kIntentionallyExcessiveDelayMs = 1000 * 1000 * 1000;
};

namespace {

class PeerConnectionTrackerProxyForTesting
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  explicit PeerConnectionTrackerProxyForTesting(
      WebRtcEventLogManagerTestBase* test)
      : test_(test) {}

  ~PeerConnectionTrackerProxyForTesting() override = default;

  void SetWebRtcEventLoggingState(const PeerConnectionKey& key,
                                  bool event_logging_enabled) override {
    test_->SetWebRtcEventLoggingState(key, event_logging_enabled);
  }

 private:
  WebRtcEventLogManagerTestBase* const test_;
};

#if defined(OS_POSIX)
void RemoveWritePermissionsFromDirectory(const base::FilePath& path) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  permissions &= ~write_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}
#endif  // defined(OS_POSIX)

// The factory for the following fake uploader produces a sequence of uploaders
// which fail the test if given a file other than that which they expect. The
// factory itself likewise fails the test if destroyed before producing all
// expected uploaders, or if it's asked for more uploaders than it expects to
// create. This allows us to test for sequences of uploads.
class FileListExpectingWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    Factory(std::list<WebRtcLogFileInfo>* expected_files,
            bool result,
            base::RunLoop* run_loop)
        : result_(result), run_loop_(run_loop) {
      expected_files_.swap(*expected_files);
      if (expected_files_.empty()) {
        run_loop_->QuitWhenIdle();
      }
    }

    ~Factory() override { EXPECT_TRUE(expected_files_.empty()); }

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) override {
      if (expected_files_.empty()) {
        EXPECT_FALSE(true);  // More files uploaded than expected.
      } else {
        EXPECT_EQ(log_file.path, expected_files_.front().path);
        // Because LoadProfiles() and UnloadProfiles() mess up the
        // BrowserContextId in ways that would not happen in production,
        // we cannot verify |log_file.browser_context_id| is correct.
        // This is unimportant to the test.

        base::DeleteFile(log_file.path, false);
        expected_files_.pop_front();
      }

      if (expected_files_.empty()) {
        run_loop_->QuitWhenIdle();
      }

      return std::make_unique<FileListExpectingWebRtcEventLogUploader>(
          log_file, result_, std::move(callback));
    }

   private:
    std::list<WebRtcLogFileInfo> expected_files_;
    const bool result_;
    base::RunLoop* const run_loop_;
  };

  // The logic is in the factory; the uploader just reports success so that the
  // next file may become eligible for uploading.
  FileListExpectingWebRtcEventLogUploader(const WebRtcLogFileInfo& log_file,
                                          bool result,
                                          UploadResultCallback callback)
      : log_file_(log_file) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), log_file_.path, result));
  }

  ~FileListExpectingWebRtcEventLogUploader() override = default;

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override {
    return log_file_;
  }

  bool Cancel() override {
    NOTREACHED() << "Incompatible with this kind of test.";
    return true;
  }

 private:
  const WebRtcLogFileInfo log_file_;
};

}  // namespace

TEST_F(WebRtcEventLogManagerTest, PeerConnectionAddedReturnsTrue) {
  EXPECT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionAddedReturnsFalseIfAlreadyAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_FALSE(PeerConnectionAdded(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest, PeerConnectionRemovedReturnsTrue) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionRemovedReturnsFalseIfNeverAdded) {
  EXPECT_FALSE(PeerConnectionRemoved(rph_->GetID(), kLid));
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionRemovedReturnsFalseIfAlreadyRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  EXPECT_FALSE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest, EnableLocalLoggingReturnsTrue) {
  EXPECT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       EnableLocalLoggingReturnsFalseIfCalledWhenAlreadyEnabled) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_FALSE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsTrue) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsIfNeverEnabled) {
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsIfAlreadyDisabled) {
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseWhenAllLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  // Note that EnableLocalLogging() and StartRemoteLogging() weren't called.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseForUnknownPeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  // Note that PeerConnectionAdded() wasn't called.
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsLocalTrueWhenPcKnownAndLocalLoggingOn) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(true, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsRemoteTrueWhenPcKnownAndRemoteLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(false, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsTrueAndTrueeWhenAllLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(true, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedNotCalledIfLocalLoggingEnabledWithoutPeerConnections) {
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedNotCalledIfLocalLoggingDisabledWithoutPeerConnections) {
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForPeerConnectionAddedAndLocalLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForLocalLoggingEnabledAndPeerConnectionAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterLocalLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterPeerConnectionRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreatesEmptyFileWhenStarted) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectLocalFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreateAndWriteToFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log = "To strive, to seek, to find, and not to yield.";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectLocalFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleWritesToSameFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
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

  ExpectLocalFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFileSizeLimitNotExceeded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "There lies the port; the vessel puffs her sail:";
  const size_t file_size_limit_bytes = log.length() / 2;

  ASSERT_TRUE(EnableLocalLogging(file_size_limit_bytes));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Failure is reported, because not everything could be written. The file
  // will also be closed.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, false));

  // Additional calls to Write() have no effect.
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "ignored"),
            std::make_pair(false, false));

  ExpectLocalFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityOverUnlimitedFileSizes) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
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

  ExpectLocalFileContents(*file_path, log1 + log2);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogNoWriteAfterLogStopped) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log_before = "log_before_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log_before),
            std::make_pair(true, false));
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  const std::string log_after = "log_after_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log_after),
            std::make_pair(false, false));

  ExpectLocalFileContents(*file_path, log_before);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogOnlyWritesTheLogsAfterStarted) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Calls to Write() before the log was started are ignored.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  const std::string log1 = "The lights begin to twinkle from the rocks:";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log1),
            std::make_pair(false, false));
  ASSERT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  base::Optional<base::FilePath> file_path;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
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
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectLocalFileContents(*file_path, log2);
}

// Note: This test also covers the scenario LocalLogExistingFilesNotOverwritten,
// which is therefore not explicitly tested.
TEST_F(WebRtcEventLogManagerTest, LocalLoggingRestartCreatesNewFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  const std::vector<std::string> logs = {"<setup>", "<punchline>", "<encore>"};
  std::vector<base::Optional<PeerConnectionKey>> keys(logs.size());
  std::vector<base::Optional<base::FilePath>> file_paths(logs.size());

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  for (size_t i = 0; i < logs.size(); ++i) {
    ON_CALL(local_observer_, OnLocalLogStarted(_, _))
        .WillByDefault(Invoke(SaveKeyAndFilePathTo(&keys[i], &file_paths[i])));
    ASSERT_TRUE(EnableLocalLogging());
    ASSERT_TRUE(keys[i]);
    ASSERT_EQ(*keys[i], key);
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, logs[i]),
              std::make_pair(true, false));
    ASSERT_TRUE(DisableLocalLogging());
  }

  for (size_t i = 0; i < logs.size(); ++i) {
    ExpectLocalFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleActiveFiles) {
  ASSERT_TRUE(EnableLocalLogging());

  std::list<MockRenderProcessHost> rphs;
  for (size_t i = 0; i < 3; ++i) {
    rphs.emplace_back(browser_context_);
  }

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.push_back(GetPeerConnectionKey(&rph, kLid));
  }

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(local_observer_, OnLocalLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(std::to_string(rph_->GetID()) + std::to_string(kLid));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(true, false));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  ASSERT_TRUE(DisableLocalLogging());

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectLocalFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogLimitActiveLocalLogFiles) {
  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kMaxLocalLogFiles));
}

// When a log reaches its maximum size limit, it is closed, and no longer
// counted towards the limit.
TEST_F(WebRtcEventLogManagerTest, LocalLogFilledLogNotCountedTowardsLogsLimit) {
  const std::string log = "very_short_log";
  ASSERT_TRUE(EnableLocalLogging(log.size()));

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  // By writing to one of the logs, we fill it and end up closing it, allowing
  // an additional log to be written.
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), 0, log),
            std::make_pair(true, false));

  // We now have room for one additional log.
  const auto last_key = GetPeerConnectionKey(rph_.get(), kMaxLocalLogFiles);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  }

  // When one peer connection is removed, one log is stopped, thereby allowing
  // an additional log to be opened.
  EXPECT_CALL(local_observer_,
              OnLocalLogStopped(GetPeerConnectionKey(rph_.get(), 0)))
      .Times(1);
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), 0));

  // We now have room for one additional log.
  const auto last_key = GetPeerConnectionKey(rph_.get(), kMaxLocalLogFiles);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogIllegalPath) {
  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  const base::FilePath illegal_path(FILE_PATH_LITERAL(":!@#$%|`^&*\\/"));
  EXPECT_TRUE(EnableLocalLogging(illegal_path));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));
}

#if defined(OS_POSIX)
TEST_F(WebRtcEventLogManagerTest, LocalLogLegalPathWithoutPermissionsSanity) {
  RemoveWritePermissionsFromDirectory(local_logs_base_dir_.GetPath());

  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  EXPECT_TRUE(EnableLocalLogging(local_logs_base_path_));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  // Write() has no effect (but is handled gracefully).
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid,
                                  "Why did the chicken cross the road?"),
            std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  // Logging was enabled, even if it had no effect because of the lacking
  // permissions; therefore, the operation of disabling it makes sense.
  EXPECT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));
}
#endif  // defined(OS_POSIX)

TEST_F(WebRtcEventLogManagerTest, LocalLogEmptyStringHandledGracefully) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  base::Optional<base::FilePath> file_path;

  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); ++i) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, logs[i]),
              std::make_pair(true, false));
  }
  ASSERT_TRUE(DisableLocalLogging());

  ExpectLocalFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFilenameMatchesExpectedFormat) {
  using StringType = base::FilePath::StringType;

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
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
      local_logs_base_dir_.GetPath().Append(user_defined);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path = local_logs_base_path;
  expected_path = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + IntToStringType(kLid));
  expected_path = expected_path.AddExtension(FILE_PATH_LITERAL("log"));

  EXPECT_EQ(file_path, expected_path);
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogFilenameMatchesExpectedFormatRepeatedFilename) {
  using StringType = base::FilePath::StringType;

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path_1;
  base::Optional<base::FilePath> file_path_2;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
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
      local_logs_base_dir_.GetPath().Append(user_defined_portion);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path_1);
  ASSERT_FALSE(file_path_1->empty());

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].log
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path_1 = local_logs_base_path;
  expected_path_1 = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + IntToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + IntToStringType(kLid));
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
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _)).Times(0);
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedNotCalledIfRemoteLoggingNotEnabled) {
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStartedCalledIfRemoteLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedCalledIfRemoteLoggingEnabledThenPcRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       BrowserContextInitializationCreatesDirectoryForRemoteLogs) {
  auto* browser_context = CreateBrowserContext();
  const base::FilePath remote_logs_path = RemoteBoundLogsDir(browser_context);
  EXPECT_TRUE(base::DirectoryExists(remote_logs_path));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnknownPeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), 0);
  std::string error_message;
  EXPECT_FALSE(
      StartRemoteLogging(key.render_process_id, "id", nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnknownPeerConnectionId) {
  const auto key = GetPeerConnectionKey(rph_.get(), 0);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, "real_id"));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, "wrong_id", nullptr,
                                  &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsTrueIfKnownPeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  EXPECT_TRUE(StartRemoteLogging(key.render_process_id, id));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfRestartAttempt) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, id));
  std::string error_message;
  EXPECT_FALSE(
      StartRemoteLogging(key.render_process_id, id, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureAlreadyLogging);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnlimitedFileSize) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, id,
                                  kWebRtcEventLogManagerUnlimitedFileSize,
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureUnlimitedSizeDisallowed);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsTrueIfFileSizeAtOrBelowLimit) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  EXPECT_TRUE(StartRemoteLogging(key.render_process_id, id,
                                 kMaxRemoteLogFileSizeBytes));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfExcessivelyLargeFileSize) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, id,
                                  kMaxRemoteLogFileSizeBytes + 1, nullptr,
                                  &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureMaxSizeTooLarge);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfPeerConnectionAlreadyClosed) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string id = "id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, id));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  std::string error_message;
  EXPECT_FALSE(
      StartRemoteLogging(key.render_process_id, id, nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingDoesNotReturnIdWhenUnsuccessful) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string pc_id = "pc_id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, pc_id));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  std::string log_id;
  ASSERT_FALSE(StartRemoteLogging(key.render_process_id, pc_id, &log_id));

  EXPECT_TRUE(log_id.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsLegalIdWhenSuccessful) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  const std::string pc_id = "pc_id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, pc_id));

  std::string log_id;
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, pc_id, &log_id));

  EXPECT_EQ(log_id.size(), 32u);
  EXPECT_EQ(log_id.find_first_not_of("0123456789ABCDEF"), std::string::npos);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingSavesToFileWithCorrectFileNameFormat) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string pc_id = "pc_id";  // For explicitness' sake.
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid, pc_id));

  std::string log_id;
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, pc_id, &log_id));

  const base::FilePath expected_filename =
      base::FilePath(kRemoteBoundWebRtcEventLogFileNamePrefix)
          .InsertBeforeExtensionASCII(log_id)
          .AddExtension(kRemoteBoundWebRtcEventLogExtension);
  EXPECT_EQ(file_path->BaseName(), expected_filename);
}

TEST_F(WebRtcEventLogManagerTest, StartRemoteLoggingCreatesEmptyFile) {
  base::Optional<base::FilePath> file_path;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));
  ASSERT_TRUE(StartRemoteLogging(rph_->GetID(), GetUniqueId(key)));

  ExpectRemoteFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileCreatedInCorrectDirectory) {
  // Set up separate browser contexts; each one will get one log.
  constexpr size_t kLogsNum = 3;
  TestingProfile* browser_contexts[kLogsNum];
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (size_t i = 0; i < kLogsNum; ++i) {
    auto* const browser_context = CreateBrowserContext();
    browser_contexts[i] = browser_context;
    rphs.emplace_back(std::make_unique<MockRenderProcessHost>(browser_context));
  }

  // Prepare to store the logs' paths in distinct memory locations.
  base::Optional<base::FilePath> file_paths[kLogsNum];
  for (size_t i = 0; i < kLogsNum; ++i) {
    const auto key = GetPeerConnectionKey(rphs[i].get(), kLid);
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
        .Times(1)
        .WillOnce(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  // Start one log for each browser context.
  for (const auto& rph : rphs) {
    const auto key = GetPeerConnectionKey(&*rph, kLid);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  }

  // All log files must be created in their own context's directory.
  for (size_t i = 0; i < arraysize(browser_contexts); ++i) {
    ASSERT_TRUE(file_paths[i]);
    EXPECT_TRUE(browser_contexts[i]->GetPath().IsParent(*file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingSanityIfDuplicateIdsInDifferentRendererProcesses) {
  std::unique_ptr<MockRenderProcessHost> rphs[2] = {
      std::make_unique<MockRenderProcessHost>(browser_context_),
      std::make_unique<MockRenderProcessHost>(browser_context_),
  };

  PeerConnectionKey keys[2] = {GetPeerConnectionKey(rphs[0].get(), 0),
                               GetPeerConnectionKey(rphs[1].get(), 0)};

  // The ID is shared, but that's not a problem, because the renderer process
  // are different.
  const std::string id = "shared_id";
  ASSERT_TRUE(PeerConnectionAdded(keys[0].render_process_id, keys[0].lid, id));
  ASSERT_TRUE(PeerConnectionAdded(keys[1].render_process_id, keys[1].lid, id));

  // Make sure the logs get written to separate files.
  base::Optional<base::FilePath> file_paths[2];
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _))
        .Times(1)
        .WillOnce(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  EXPECT_TRUE(StartRemoteLogging(keys[0].render_process_id, id));
  EXPECT_TRUE(StartRemoteLogging(keys[1].render_process_id, id));

  EXPECT_TRUE(file_paths[0]);
  EXPECT_TRUE(file_paths[1]);
  EXPECT_NE(file_paths[0], file_paths[1]);
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteWritesToTheRemoteBoundFile) {
  base::Optional<base::FilePath> file_path;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));

  const char* const log = "1 + 1 = 3";
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, true));

  ExpectRemoteFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, WriteToBothLocalAndRemoteFiles) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  base::Optional<base::FilePath> local_path;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&local_path)));

  base::Optional<base::FilePath> remote_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&remote_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));

  ASSERT_TRUE(local_path);
  ASSERT_FALSE(local_path->empty());
  ASSERT_TRUE(remote_path);
  ASSERT_FALSE(remote_path->empty());

  const char* const log = "logloglog";
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(true, true));

  // Ensure the flushing of the file to disk before attempting to read them.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectLocalFileContents(*local_path, log);
  ExpectRemoteFileContents(*remote_path, log);
}

TEST_F(WebRtcEventLogManagerTest, MultipleWritesToSameRemoteBoundLogfile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string logs[] = {"ABC", "DEF", "XYZ"};
  for (const std::string& log : logs) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
              std::make_pair(false, true));
  }

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectRemoteFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileSizeLimitNotExceeded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "tpyo";
  const size_t file_size_limit_bytes = log.length() / 2;

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                 file_size_limit_bytes));

  // Failure is reported, because not everything could be written. The file
  // will also be closed.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, false));

  // Additional calls to Write() have no effect.
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "ignored"),
            std::make_pair(false, false));

  ExpectRemoteFileContents(*file_path, "");
}

TEST_F(WebRtcEventLogManagerTest,
       LogMultipleActiveRemoteLogsSameBrowserContext) {
  const std::vector<PeerConnectionKey> keys = {
      GetPeerConnectionKey(rph_.get(), 0), GetPeerConnectionKey(rph_.get(), 1),
      GetPeerConnectionKey(rph_.get(), 2)};

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(
        StartRemoteLogging(keys[i].render_process_id, GetUniqueId(keys[i])));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(std::to_string(rph_->GetID()) + std::to_string(i));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest,
       LogMultipleActiveRemoteLogsDifferentBrowserContexts) {
  constexpr size_t kLogsNum = 3;
  TestingProfile* browser_contexts[kLogsNum];
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (size_t i = 0; i < kLogsNum; ++i) {
    TestingProfile* const browser_context = CreateBrowserContext();
    browser_contexts[i] = browser_context;
    rphs.emplace_back(std::make_unique<MockRenderProcessHost>(browser_context));
  }

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.push_back(GetPeerConnectionKey(rph.get(), kLid));
  }

  std::vector<base::Optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(
        StartRemoteLogging(keys[i].render_process_id, GetUniqueId(keys[i])));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(std::to_string(rph_->GetID()) + std::to_string(i));
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, DifferentRemoteLogsMayHaveDifferentMaximums) {
  const std::string logs[2] = {"abra", "cadabra"};
  std::vector<base::Optional<base::FilePath>> file_paths(arraysize(logs));
  std::vector<PeerConnectionKey> keys;
  for (size_t i = 0; i < arraysize(logs); ++i) {
    keys.push_back(GetPeerConnectionKey(rph_.get(), i));
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ASSERT_TRUE(PeerConnectionAdded(keys[i].render_process_id, keys[i].lid));
    ASSERT_TRUE(StartRemoteLogging(keys[i].render_process_id,
                                   GetUniqueId(keys[i]), logs[i].length()));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    // The write is successful, but the file closed, indicating that the maximum
    // file size has been reached.
    EXPECT_CALL(remote_observer_, OnRemoteLogStopped(keys[i])).Times(1);
    ASSERT_EQ(
        OnWebRtcEventLogWrite(keys[i].render_process_id, keys[i].lid, logs[i]),
        std::make_pair(false, true));
    ASSERT_TRUE(file_paths[i]);
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileClosedWhenCapacityReached) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "Let X equal X.";

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                 log.length()));
  ASSERT_TRUE(file_path);

  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, true));
}

#if defined(OS_POSIX)
// TODO(crbug.com/775415): Add unit tests for lacking read permissions when
// looking to upload the file.
TEST_F(WebRtcEventLogManagerTest,
       FailureToCreateRemoteLogsDirHandledGracefully) {
  const base::FilePath browser_context_dir = browser_context_->GetPath();
  const base::FilePath remote_logs_path = RemoteBoundLogsDir(browser_context_);

  // Unload the profile, delete its remove logs directory, and remove write
  // permissions from it, thereby preventing it from being created again.
  UnloadProfiles();
  ASSERT_TRUE(base::DeleteFile(remote_logs_path, /*recursive=*/true));
  RemoveWritePermissionsFromDirectory(browser_context_dir);

  // Graceful handling by BrowserContext::EnableForBrowserContext, despite
  // failing to create the remote logs' directory..
  LoadProfiles();
  EXPECT_FALSE(base::DirectoryExists(remote_logs_path));

  // Graceful handling of PeerConnectionAdded: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.)
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  // Graceful handling of StartRemoteLogging: False returned because it's
  // impossible to write the log to a file.
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(rph_->GetID(), GetUniqueId(key), nullptr,
                                  &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);

  // Graceful handling of OnWebRtcEventLogWrite: False returned because the
  // log could not be written at all, let alone in its entirety.
  const char* const log = "This is not a log.";
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, log),
            std::make_pair(false, false));

  // Graceful handling of PeerConnectionRemoved: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.
  EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest, GracefullyHandleFailureToStartRemoteLogFile) {
  // WebRTC logging will not be turned on.
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _)).Times(0);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);

  // Remove write permissions from the directory.
  const base::FilePath remote_logs_path = RemoteBoundLogsDir(browser_context_);
  ASSERT_TRUE(base::DirectoryExists(remote_logs_path));
  RemoveWritePermissionsFromDirectory(remote_logs_path);

  // StartRemoteLogging() will now fail.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "abc"),
            std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}
#endif  // defined(OS_POSIX)

TEST_F(WebRtcEventLogManagerTest, RemoteLogLimitActiveLogFiles) {
  for (int i = 0; i < kMaxActiveRemoteLogFiles + 1; ++i) {
    ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), i));
  }

  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  }

  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _)).Times(0);
  const auto new_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_FALSE(
      StartRemoteLogging(new_key.render_process_id, GetUniqueId(new_key)));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogFilledLogNotCountedTowardsLogsLimit) {
  const std::string log = "very_short_log";

  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                   log.length()));
  }

  // By writing to one of the logs until it reaches capacity, we fill it,
  // causing it to close, therefore allowing an additional log.
  EXPECT_EQ(OnWebRtcEventLogWrite(rph_->GetID(), 0, log),
            std::make_pair(false, true));

  // We now have room for one additional log.
  const auto new_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(new_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(new_key.render_process_id, new_key.lid));
  ASSERT_TRUE(
      StartRemoteLogging(new_key.render_process_id, GetUniqueId(new_key)));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  }

  // By removing a peer connection associated with one of the logs, we allow
  // an additional log.
  ASSERT_TRUE(PeerConnectionRemoved(rph_->GetID(), 0));

  // We now have room for one additional log.
  const auto last_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(PeerConnectionAdded(last_key.render_process_id, last_key.lid));
  ASSERT_TRUE(
      StartRemoteLogging(last_key.render_process_id, GetUniqueId(last_key)));
}

TEST_F(WebRtcEventLogManagerTest,
       ActiveLogsForBrowserContextCountedTowardsItsPendingsLogsLimit) {
  SuppressUploading();

  // Produce kMaxPendingRemoteLogFiles pending logs.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
    ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  // It is now impossible to start another *active* log for that BrowserContext,
  // because we have too many pending logs (and active logs become pending
  // once completed).
  const auto forbidden =
      GetPeerConnectionKey(rph_.get(), kMaxPendingRemoteLogFiles);
  ASSERT_TRUE(PeerConnectionAdded(forbidden.render_process_id, forbidden.lid));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(forbidden.render_process_id,
                                  GetUniqueId(forbidden), nullptr,
                                  &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);
}

TEST_F(WebRtcEventLogManagerTest,
       ObserveLimitOnMaximumPendingLogsPerBrowserContext) {
  SuppressUploading();

  // Create additional BrowserContexts for the test.
  TestingProfile* browser_contexts[2] = {CreateBrowserContext(),
                                         CreateBrowserContext()};
  std::unique_ptr<MockRenderProcessHost> rphs[2] = {
      std::make_unique<MockRenderProcessHost>(browser_contexts[0]),
      std::make_unique<MockRenderProcessHost>(browser_contexts[1])};

  // Allowed to start kMaxPendingRemoteLogFiles for each BrowserContext.
  // Specifically, we can do it for the first BrowserContext.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rphs[0].get(), i);
    // The log could be opened:
    ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
    ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
    // The log changes state from ACTIVE to PENDING:
    EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  }

  // Not allowed to start any more remote-bound logs for the BrowserContext on
  // which the limit was reached.
  const auto key0 =
      GetPeerConnectionKey(rphs[0].get(), kMaxPendingRemoteLogFiles);
  ASSERT_TRUE(PeerConnectionAdded(key0.render_process_id, key0.lid));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key0.render_process_id, GetUniqueId(key0),
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);

  // Other BrowserContexts aren't limit by the previous one's limit.
  const auto key1 = GetPeerConnectionKey(rphs[1].get(), 0);
  ASSERT_TRUE(PeerConnectionAdded(key1.render_process_id, key1.lid));
  EXPECT_TRUE(StartRemoteLogging(key1.render_process_id, GetUniqueId(key1)));
}

TEST_F(WebRtcEventLogManagerTest,
       LogsFromPreviousSessionBecomePendingLogsWhenBrowserContextInitialized) {
  // Unload the profile, but remember where it stores its files.
  const base::FilePath browser_context_path = browser_context_->GetPath();
  const base::FilePath remote_logs_dir = RemoteBoundLogsDir(browser_context_);
  UnloadProfiles();

  // Seed the remote logs' directory with log files, simulating the
  // creation of logs in a previous session.
  std::list<WebRtcLogFileInfo> expected_files;
  ASSERT_TRUE(CreateDirectory(remote_logs_dir));

  for (size_t i = 0; i < kMaxPendingRemoteBoundWebRtcEventLogs; ++i) {
    const base::FilePath file_path =
        remote_logs_dir.Append(IntToStringType(i))
            .AddExtension(kRemoteBoundWebRtcEventLogExtension);
    constexpr int file_flags = base::File::FLAG_CREATE |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_EXCLUSIVE_WRITE;
    base::File file(file_path, file_flags);
    ASSERT_TRUE(file.IsValid() && file.created());
    expected_files.emplace_back(browser_context_id_, file_path,
                                GetLastModificationTime(file_path));
  }

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  LoadProfiles();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTest,
       WhenPeerConnectionRemovedFinishedRemoteLogUploadedAndFileDeleted) {
  // |upload_result| show that the files are deleted independent of the
  // upload's success / failure.
  const bool upload_result = GetParam();

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, upload_result, &run_loop));

  // Peer connection removal triggers next upload.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  WaitForPendingTasks(&run_loop);

  EXPECT_TRUE(base::IsDirectoryEmpty(RemoteBoundLogsDir(browser_context_)));
}

TEST_P(WebRtcEventLogManagerTest, DestroyedRphTriggersLogUpload) {
  // |upload_result| show that the files are deleted independent of the
  // upload's success / failure.
  const bool upload_result = GetParam();

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, upload_result, &run_loop));

  // RPH destruction stops all active logs and triggers next upload.
  rph_.reset();

  WaitForPendingTasks(&run_loop);

  EXPECT_TRUE(base::IsDirectoryEmpty(RemoteBoundLogsDir(browser_context_)));
}

// Note that SuppressUploading() and UnSuppressUploading() use the behavior
// guaranteed by this test.
TEST_F(WebRtcEventLogManagerTest, UploadOnlyWhenNoActivePeerConnections) {
  const auto untracked = GetPeerConnectionKey(rph_.get(), 0);
  const auto tracked = GetPeerConnectionKey(rph_.get(), 1);

  // Suppresses the uploading of the "tracked" peer connection's log.
  ASSERT_TRUE(PeerConnectionAdded(untracked.render_process_id, untracked.lid));

  // The tracked peer connection's log is not uploaded when finished, because
  // another peer connection is still active.
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(tracked, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(tracked.render_process_id, tracked.lid));
  ASSERT_TRUE(
      StartRemoteLogging(tracked.render_process_id, GetUniqueId(tracked)));
  ASSERT_TRUE(log_file);
  ASSERT_TRUE(PeerConnectionRemoved(tracked.render_process_id, tracked.lid));

  // Perform another action synchronously, so that we may be assured that the
  // observer's lack of callbacks was not a timing fluke.
  OnWebRtcEventLogWrite(untracked.render_process_id, untracked.lid, "Ook!");

  // Having been convinced that |tracked|'s log was not uploded while
  // |untracked| was active, close |untracked| and see that |tracked|'s log
  // is now uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_uploads = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_uploads, true, &run_loop));
  ASSERT_TRUE(
      PeerConnectionRemoved(untracked.render_process_id, untracked.lid));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest, UploadOrderDependsOnLastModificationTime) {
  constexpr size_t kProfilesNum = 3;
  const std::string profile_names[kProfilesNum] = {"a", "b", "c"};

  // Create profiles. This creates their directories.
  base::FilePath remote_logs_dirs[kProfilesNum];
  BrowserContext* browser_contexts[kProfilesNum];
  for (size_t i = 0; i < kProfilesNum; ++i) {
    browser_contexts[i] = CreateBrowserContext(profile_names[i]);
    remote_logs_dirs[i] = RemoteBoundLogsDir(browser_contexts[i]);
  }

  // Unload the profiles, so that whatever files we add into their directory
  // could be discovered by EnableForBrowserContext when we reload them.
  UnloadProfiles();

  // Seed the directories with log files.
  base::FilePath file_paths[kProfilesNum];
  for (size_t i = 0; i < kProfilesNum; ++i) {
    ASSERT_TRUE(base::DirectoryExists(remote_logs_dirs[i]));
    file_paths[i] = remote_logs_dirs[i].AppendASCII("file").AddExtension(
        kRemoteBoundWebRtcEventLogExtension);
    ASSERT_TRUE(!base::PathExists(file_paths[i]));
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
  const size_t permutation[kProfilesNum] = {2, 0, 1};
  std::list<WebRtcLogFileInfo> expected_files;
  base::Time mod_time =
      base::Time::Now() - base::TimeDelta::FromSeconds(kProfilesNum);
  for (size_t i = 0; i < kProfilesNum; ++i) {
    mod_time += base::TimeDelta::FromSeconds(1);  // Back to the future.
    const base::FilePath& path = file_paths[permutation[i]];
    ASSERT_TRUE(base::TouchFile(path, shared_last_accessed, mod_time));
    expected_files.emplace_back(GetBrowserContextId(browser_contexts[i]), path,
                                GetLastModificationTime(path));
  }

  // Recognize the files as pending files by initializing their BrowserContexts.
  // We keep uploading suppressed so as to avoid them being uploaded in order of
  // loading, rather than in order of date.
  LoadProfiles();
  SuppressUploading();
  for (size_t i = 0; i < kProfilesNum; ++i) {
    CreateBrowserContext(profile_names[i]);  // Owned by the profile manager.
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
  constexpr size_t kExpired = 0;
  constexpr size_t kFresh = 1;
  DCHECK_GE(kMaxPendingRemoteBoundWebRtcEventLogs, 2u)
      << "Please restructure the test to use separate browser contexts.";

  const base::FilePath remote_logs_dir = RemoteBoundLogsDir(browser_context_);

  UnloadProfiles();

  base::FilePath file_paths[2];
  for (size_t i = 0; i < 2; ++i) {
    file_paths[i] = remote_logs_dir.Append(IntToStringType(i))
                        .AddExtension(kRemoteBoundWebRtcEventLogExtension);
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

  // Show that the expired file is not uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {
      WebRtcLogFileInfo(browser_context_id_, file_paths[kFresh],
                        GetLastModificationTime(file_paths[kFresh]))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Recognize the files as pending by initializing their BrowserContext.
  LoadProfiles();

  WaitForPendingTasks(&run_loop);

  // Both the uploaded file as well as the expired file have no been removed
  // from local disk.
  EXPECT_TRUE(base::IsDirectoryEmpty(RemoteBoundLogsDir(browser_context_)));
}

// TODO(crbug.com/775415): Add a test showing that a file expiring while another
// is being uploaded, is not uploaded after the current upload is completed.
// This is significant because Chrome might stay up for a long time.

TEST_F(WebRtcEventLogManagerTest, RemoteLogEmptyStringHandledGracefully) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  base::Optional<base::FilePath> file_path;

  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); ++i) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, logs[i]),
              std::make_pair(false, true));
  }
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectRemoteFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

#if defined(OS_POSIX)
TEST_F(WebRtcEventLogManagerTest,
       UnopenedRemoteLogFilesNotCountedTowardsActiveLogsLimit) {
  TestingProfile* browser_contexts[2];
  std::unique_ptr<MockRenderProcessHost> rphs[2];
  for (size_t i = 0; i < 2; ++i) {
    browser_contexts[i] = CreateBrowserContext();
    rphs[i] = std::make_unique<MockRenderProcessHost>(browser_contexts[i]);
  }

  constexpr size_t without_permissions = 0;
  constexpr size_t with_permissions = 1;

  // Remove write permissions from one directory.
  const base::FilePath permissions_lacking_remote_logs_path =
      RemoteBoundLogsDir(browser_contexts[without_permissions]);
  ASSERT_TRUE(base::DirectoryExists(permissions_lacking_remote_logs_path));
  RemoveWritePermissionsFromDirectory(permissions_lacking_remote_logs_path);

  // Fail to start a log associated with the permission-lacking directory.
  const auto without_permissions_key =
      GetPeerConnectionKey(rphs[without_permissions].get(), 0);
  ASSERT_TRUE(PeerConnectionAdded(without_permissions_key.render_process_id,
                                  without_permissions_key.lid));
  std::string error_message;
  ASSERT_FALSE(StartRemoteLogging(without_permissions_key.render_process_id,
                                  GetUniqueId(without_permissions_key), nullptr,
                                  &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);

  // Show that this was not counted towards the limit of active files.
  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto with_permissions_key =
        GetPeerConnectionKey(rphs[with_permissions].get(), i);
    ASSERT_TRUE(PeerConnectionAdded(with_permissions_key.render_process_id,
                                    with_permissions_key.lid));
    EXPECT_TRUE(StartRemoteLogging(with_permissions_key.render_process_id,
                                   GetUniqueId(with_permissions_key)));
  }
}
#endif  // defined(OS_POSIX)

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenLocalEnabledWithoutPeerConnection) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenPeerConnectionButNoLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenLocalEnabledThenPeerConnectionAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenPeerConnectionAddedThenLocalEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenRemoteLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);
}

TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenLocalLoggingStopped) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, false);
}

// #1 - Local logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenPeerConnectionRemoved1) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, false);
}

// #2 - Remote logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenPeerConnectionRemoved2) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, false);
}

// #1 - Local logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging1) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// #2 - Remote logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging2) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingWhenRemoteLoggingEnabledDoesNotStopWebRtcLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());

  // Cleanup
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, false);
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingAfterPcRemovalHasNoEffectOnWebRtcLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, true);

  // Test
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ExpectWebRtcStateChangeInstruction(key.render_process_id, key.lid, false);
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// Once a peer connection with a given key was removed, it may not again be
// added. But, if this impossible case occurs, WebRtcEventLogManager will
// not crash.
TEST_F(WebRtcEventLogManagerTest, SanityOverRecreatingTheSamePeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log1");
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  OnWebRtcEventLogWrite(rph_->GetID(), kLid, "log2");
}

// The logs would typically be binary. However, the other tests only cover ASCII
// characters, for readability. This test shows that this is not a problem.
TEST_F(WebRtcEventLogManagerTest, LogAllPossibleCharacters) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> local_log_file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&local_log_file_path)));

  base::Optional<base::FilePath> remote_log_file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&remote_log_file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(local_log_file_path);
  ASSERT_FALSE(local_log_file_path->empty());
  ASSERT_TRUE(remote_log_file_path);
  ASSERT_FALSE(remote_log_file_path->empty());

  std::string all_chars;
  for (size_t i = 0; i < 256; ++i) {
    all_chars += static_cast<uint8_t>(i);
  }
  ASSERT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, all_chars),
            std::make_pair(true, true));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  ExpectLocalFileContents(*local_log_file_path, all_chars);
  ExpectRemoteFileContents(*remote_log_file_path, all_chars);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogsClosedWhenRenderProcessHostExits) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());

  // The expectation for OnLocalLogStopped() will be saturated by this
  // destruction of the RenderProcessHost, which triggers an implicit
  // removal of all PeerConnections associated with it.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  rph_.reset();
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogsClosedWhenRenderProcessHostExits) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));

  // The expectation for OnRemoteLogStopped() will be saturated by this
  // destruction of the RenderProcessHost, which triggers an implicit
  // removal of all PeerConnections associated with it.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  rph_.reset();
}

// Once a RenderProcessHost exits/crashes, its PeerConnections are removed,
// which means that they can no longer suppress an upload.
TEST_F(WebRtcEventLogManagerTest,
       RenderProcessHostExitCanRemoveUploadSuppression) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // The above removal is not sufficient to trigger an upload (so the test will
  // not be flaky). It's only once we destroy the RPH with which the suppressing
  // PeerConnection is associated, that upload will take place.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *file_path, GetLastModificationTime(*file_path))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // We destroy the RPH without explicitly removing its PeerConnection (unlike
  // a call to UnsuppressUploading()).
  upload_suppressing_rph_.reset();

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionAddedOverDestroyedRphReturnsFalse) {
  const int render_process_id = rph_->GetID();
  rph_.reset();
  EXPECT_FALSE(PeerConnectionAdded(render_process_id, kLid));
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionRemovedOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  // Test
  rph_.reset();
  EXPECT_FALSE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       PeerConnectionStoppedOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  // Test
  rph_.reset();
  EXPECT_FALSE(PeerConnectionStopped(key.render_process_id, key.lid));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  // Test
  rph_.reset();
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureGeneric);
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteOverDestroyedRphReturnsFalseAndFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(EnableLocalLogging());

  // Test
  rph_.reset();
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(false, false));
}

INSTANTIATE_TEST_CASE_P(UploadCompleteResult,
                        WebRtcEventLogManagerTest,
                        ::testing::Bool());

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextRemovesPendingFilesInRange) {
  SuppressUploading();

  auto* const browser_context = CreateBrowserContext("name");
  CreatePendingLogFiles(browser_context);
  auto& elements = *(pending_logs_[browser_context]);

  const base::Time earliest_mod = pending_earliest_mod_ - kEpsion;
  const base::Time latest_mod = pending_latest_mod_ + kEpsion;

  // Test - ClearCacheForBrowserContext() removed all of the files in the range.
  ClearCacheForBrowserContext(browser_context, earliest_mod, latest_mod);
  for (size_t i = 0; i < elements.file_paths.size(); ++i) {
    EXPECT_FALSE(base::PathExists(*elements.file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextCancelsActiveLogFilesIfInRange) {
  SuppressUploading();

  // Setup
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  base::Optional<base::FilePath> file_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));

  // Test
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ClearCacheForBrowserContext(
      browser_context_, base::Time::Now() - base::TimeDelta::FromHours(1),
      base::Time::Now() + base::TimeDelta::FromHours(1));
  EXPECT_FALSE(base::PathExists(*file_path));
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextCancelsFileUploadIfInRange) {
  // This factory will enforce the expectation that the upload is cancelled.
  // WebRtcEventLogUploaderImplTest.CancelOnOngoingUploadDeletesFile is in
  // charge of making sure that when the upload is cancelled, the file is
  // removed from disk.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory
  // should now be satisfied.
  ClearCacheForBrowserContext(browser_context_, mod_time - kEpsion,
                              mod_time + kEpsion);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotRemovePendingFilesOutOfRange) {
  SuppressUploading();

  auto* const browser_context = CreateBrowserContext("name");
  CreatePendingLogFiles(browser_context);
  auto& elements = *(pending_logs_[browser_context]);

  // Get a range whose intersection with the files' range is empty.
  const base::Time earliest_mod =
      pending_earliest_mod_ - base::TimeDelta::FromHours(2);
  const base::Time latest_mod =
      pending_earliest_mod_ - base::TimeDelta::FromHours(1);
  ASSERT_LT(latest_mod, pending_latest_mod_);

  // Test - ClearCacheForBrowserContext() does not remove files not in range.
  // (Range chosen to be earlier than the oldest file
  ClearCacheForBrowserContext(browser_context, earliest_mod, latest_mod);
  for (size_t i = 0; i < elements.file_paths.size(); ++i) {
    EXPECT_TRUE(base::PathExists(*elements.file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelActiveLogFilesIfOutOfRange) {
  SuppressUploading();

  // Setup
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  base::Optional<base::FilePath> file_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));

  // Test
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);
  ClearCacheForBrowserContext(
      browser_context_, base::Time::Now() - base::TimeDelta::FromHours(2),
      base::Time::Now() - base::TimeDelta::FromHours(1));
  EXPECT_TRUE(base::PathExists(*file_path));
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelFileUploadIfOutOfRange) {
  // This factory will enforce the expectation that the upload is not cancelled.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(false));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory,
  // that the upload will not be cancelled, should be shown to hold true.
  // should now be satisfied.
  ClearCacheForBrowserContext(browser_context_, mod_time + kEpsion,
                              mod_time + 2 * kEpsion);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotRemovePendingFilesFromOtherProfiles) {
  SuppressUploading();

  auto* const cleared_browser_context = CreateBrowserContext("cleared");
  CreatePendingLogFiles(cleared_browser_context);
  auto& cleared_elements = *(pending_logs_[cleared_browser_context]);

  auto* const uncleared_browser_context = CreateBrowserContext("pristine");
  CreatePendingLogFiles(uncleared_browser_context);
  auto& uncleared_elements = *(pending_logs_[uncleared_browser_context]);

  ASSERT_EQ(cleared_elements.file_paths.size(),
            uncleared_elements.file_paths.size());
  const size_t kFileCount = cleared_elements.file_paths.size();

  const base::Time earliest_mod = pending_earliest_mod_ - kEpsion;
  const base::Time latest_mod = pending_latest_mod_ + kEpsion;

  // Test - ClearCacheForBrowserContext() only removes the files which belong
  // to the cleared context.
  ClearCacheForBrowserContext(cleared_browser_context, earliest_mod,
                              latest_mod);
  for (size_t i = 0; i < kFileCount; ++i) {
    EXPECT_FALSE(base::PathExists(*cleared_elements.file_paths[i]));
    EXPECT_TRUE(base::PathExists(*uncleared_elements.file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelActiveLogsFromOtherProfiles) {
  SuppressUploading();

  // Remote-bound active log file that *will* be cleared.
  auto* const cleared_browser_context = CreateBrowserContext("cleared");
  auto cleared_rph =
      std::make_unique<MockRenderProcessHost>(cleared_browser_context);
  const auto cleared_key = GetPeerConnectionKey(cleared_rph.get(), kLid);
  base::Optional<base::FilePath> cleared_file_path =
      CreateActiveRemoteLogFile(cleared_key);

  // Remote-bound active log file that will *not* be cleared.
  auto* const uncleared_browser_context = CreateBrowserContext("pristine");
  auto uncleared_rph =
      std::make_unique<MockRenderProcessHost>(uncleared_browser_context);
  const auto uncleared_key = GetPeerConnectionKey(uncleared_rph.get(), kLid);
  base::Optional<base::FilePath> uncleared_file_path =
      CreateActiveRemoteLogFile(uncleared_key);

  // Test - ClearCacheForBrowserContext() only removes the files which belong
  // to the cleared context.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(cleared_key)).Times(1);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(uncleared_key)).Times(0);
  ClearCacheForBrowserContext(cleared_browser_context, base::Time::Min(),
                              base::Time::Max());
  EXPECT_FALSE(base::PathExists(*cleared_file_path));
  EXPECT_TRUE(base::PathExists(*uncleared_file_path));

  // Cleanup - uncleared_file_path will be closed as part of the shutdown. It
  // is time to clear its expectation.
  testing::Mock::VerifyAndClearExpectations(&remote_observer_);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelFileUploadFromOtherProfiles) {
  // This factory will enforce the expectation that the upload is not cancelled.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(false));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  base::Optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory,
  // that the upload will not be cancelled, should be shown to hold true.
  // should now be satisfied.
  const auto* const different_browser_context = CreateBrowserContext();
  ClearCacheForBrowserContext(different_browser_context, mod_time - kEpsion,
                              mod_time + kEpsion);
}

// Show that clearing browser cache, while it removes remote-bound logs, does
// not interfere with local-bound logging, even if that happens on the same PC.
TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotInterfereWithLocalLogs) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  base::Optional<base::FilePath> local_log;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&local_log)));
  ASSERT_TRUE(EnableLocalLogging());

  // This adds a peer connection for |key|, which also triggers
  // OnLocalLogStarted() on |local_observer_|.
  auto pending_remote_log = CreatePendingRemoteLogFile(key);

  // Test focus - local logging is uninterrupted.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);
  ClearCacheForBrowserContext(browser_context_, base::Time::Min(),
                              base::Time::Max());
  EXPECT_TRUE(base::PathExists(*local_log));

  // Sanity on the test itself; the remote log should have been cleared.
  ASSERT_FALSE(base::PathExists(*pending_remote_log));
}

// When cache clearing cancels the active upload, the next (non-deleted) pending
// file becomes eligible for upload.
TEST_F(WebRtcEventLogManagerTestCacheClearing,
       UploadCancellationTriggersUploadOfNextPendingFile) {
  // The first created file will start being uploaded, but then cancelled.
  // The second file will never be uploaded (deleted while pending).
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true));

  // Create the files that will be deleted when cache is cleared.
  CreatePendingRemoteLogFile(GetPeerConnectionKey(rph_.get(), 0));
  CreatePendingRemoteLogFile(GetPeerConnectionKey(rph_.get(), 1));

  // Create the not-deleted file under a different profile, to easily make sure
  // it does not fit in the ClearCacheForBrowserContext range (less fiddly than
  // a time range).
  auto* other_browser_context = CreateBrowserContext();
  auto other_rph =
      std::make_unique<MockRenderProcessHost>(other_browser_context);
  const auto key = GetPeerConnectionKey(other_rph.get(), kLid);
  base::Optional<base::FilePath> other_file = CreatePendingRemoteLogFile(key);
  ASSERT_TRUE(other_file);

  // Switch the uploader factory to one that will allow us to ensure that the
  // new file, which is not deleted, is uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {
      WebRtcLogFileInfo(GetBrowserContextId(other_browser_context), *other_file,
                        GetLastModificationTime(*other_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Clearing the cache for the first profile, should now trigger the upload
  // of the last remaining unclear pending log file - |other_file|.
  ClearCacheForBrowserContext(browser_context_, base::Time::Min(),
                              base::Time::Max());
  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityPeerConnectionAdded) {
  EXPECT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityPeerConnectionRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityPeerConnectionStopped) {
  PeerConnectionStopped(rph_->GetID(), kLid);  // No crash.
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityEnableLocalLogging) {
  ASSERT_TRUE(PeerConnectionAdded(rph_->GetID(), kLid));
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityDisableLocalLogging) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(DisableLocalLogging());
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityStartRemoteLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, GetUniqueId(key),
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureFeatureDisabled);
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
       SanityOnWebRtcEventLogWrite) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_FALSE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  EXPECT_EQ(OnWebRtcEventLogWrite(key.render_process_id, key.lid, "log"),
            std::make_pair(false, false));
}

INSTANTIATE_TEST_CASE_P(
    ExplicitlyDisable,
    WebRtcEventLogManagerTestWithRemoteLoggingDisabledOrNotEnabled,
    ::testing::Bool());

TEST_F(WebRtcEventLogManagerTestUploadSuppressionDisablingFlag,
       UploadingNotSuppressedByActivePeerConnections) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));

  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       DoNotUploadPendingLogsIfConnectedToUnsupportedNetworkType) {
  WebRtcEventLogManagerTestBase::SetUp(
      std::make_unique<content::MockNetworkConnectionTracker>(
          get_conn_type_is_sync_, unsupported_type_));

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  std::list<WebRtcLogFileInfo> empty_expected_files_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_expected_files_list, true, &run_loop));

  // Peer connection removal MAY trigger upload, depending on network.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsIfConnectedToSupportedNetworkType) {
  WebRtcEventLogManagerTestBase::SetUp(
      std::make_unique<content::MockNetworkConnectionTracker>(
          get_conn_type_is_sync_, supported_type_));

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Peer connection removal MAY trigger upload, depending on network.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsIfConnectionTypeChangesFromUnsupportedToSupported) {
  auto tracker = std::make_unique<content::MockNetworkConnectionTracker>(
      get_conn_type_is_sync_, unsupported_type_);
  content::MockNetworkConnectionTracker* mock = tracker.get();
  WebRtcEventLogManagerTestBase::SetUp(std::move(tracker));

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  // That a peer connection upload is not initiated by this point, is verified
  // by previous tests.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));
  WaitForPendingTasks();

  // Test focus - an upload will be initiated after changing the network type.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));
  mock->SetConnectionType(supported_type_);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       DoNotUploadPendingLogsAtStartupIfConnectedToUnsupportedNetworkType) {
  WebRtcEventLogManagerTestBase::SetUp(
      std::make_unique<content::MockNetworkConnectionTracker>(
          get_conn_type_is_sync_, unsupported_type_));

  UnloadProfileAndSeedPendingLog();

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  std::list<WebRtcLogFileInfo> empty_expected_files_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_expected_files_list, true, &run_loop));

  LoadProfiles();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path_);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsAtStartupIfConnectedToSupportedNetworkType) {
  WebRtcEventLogManagerTestBase::SetUp(
      std::make_unique<content::MockNetworkConnectionTracker>(
          get_conn_type_is_sync_, supported_type_));

  UnloadProfileAndSeedPendingLog();

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files_, true, &run_loop));

  LoadProfiles();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path_);

  WaitForPendingTasks(&run_loop);
}

INSTANTIATE_TEST_CASE_P(
    UploadSupportingConnectionTypes,
    WebRtcEventLogManagerTestForNetworkConnectivity,
    ::testing::Combine(
        // Wehther GetConnectionType() responds synchronously.
        ::testing::Bool(),
        // The upload-supporting network type to be used.
        ::testing::Values(network::mojom::ConnectionType::CONNECTION_ETHERNET,
                          network::mojom::ConnectionType::CONNECTION_WIFI),
        // The upload-unsupporting network type to be used.
        ::testing::Values(network::mojom::ConnectionType::CONNECTION_NONE,
                          network::mojom::ConnectionType::CONNECTION_4G)));

TEST_F(WebRtcEventLogManagerTestUploadDelay, DoNotInitiateUploadBeforeDelay) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  // Wait a bit and see that the upload was not initiated. (Due to technical
  // constraints, we cannot wait forever.)
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  event.TimedWait(base::TimeDelta::FromMilliseconds(500));

  WaitForPendingTasks(&run_loop);
}

// WhenPeerConnectionRemovedFinishedRemoteLogUploadedAndFileDeleted has some
// overlap with this, but we still include this test for explicitness and
// clarity.
TEST_F(WebRtcEventLogManagerTestUploadDelay, InitiateUploadAfterDelay) {
  SetUp(kDefaultUploadDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  base::Optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestUploadDelay,
       PeerConnectionAddedDuringDelaySuppressesUpload) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key1 = GetPeerConnectionKey(rph_.get(), 1);
  const auto key2 = GetPeerConnectionKey(rph_.get(), 2);

  ASSERT_TRUE(PeerConnectionAdded(key1.render_process_id, key1.lid));
  ASSERT_TRUE(StartRemoteLogging(key1.render_process_id, GetUniqueId(key1)));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(PeerConnectionRemoved(key1.render_process_id, key1.lid));

  // Test focus - after adding a peer connection, the conditions for the upload
  // are no longer considered to hold.
  // (Test implemented with a glimpse into the black box due to technical
  // limitations and the desire to avoid flakiness.)
  ASSERT_TRUE(PeerConnectionAdded(key2.render_process_id, key2.lid));
  EXPECT_FALSE(UploadConditionsHold());

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestUploadDelay,
       ClearCacheForBrowserContextDuringDelayCancelsItsUpload) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);

  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  ASSERT_TRUE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(PeerConnectionRemoved(key.render_process_id, key.lid));

  // Test focus - after clearing browser cache, the conditions for the upload
  // are no longer considered to hold, because the file about to be uploaded
  // was deleted.
  // (Test implemented with a glimpse into the black box due to technical
  // limitations and the desire to avoid flakiness.)
  ClearCacheForBrowserContext(browser_context_, base::Time::Min(),
                              base::Time::Max());
  EXPECT_FALSE(UploadConditionsHold());

  WaitForPendingTasks(&run_loop);
}

#else  // defined(OS_ANDROID)

class WebRtcEventLogManagerTestOnMobileDevices
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestOnMobileDevices() {
    // features::kWebRtcRemoteEventLog not defined on mobile, and can therefore
    // not be forced on. This test is here to make sure that when the feature
    // is changed to be on by default, it will still be off for mobile devices.
    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();
  }
};

TEST_F(WebRtcEventLogManagerTestOnMobileDevices, RemoteBoundLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(PeerConnectionAdded(key.render_process_id, key.lid));
  EXPECT_FALSE(StartRemoteLogging(key.render_process_id, GetUniqueId(key)));
}

#endif
