// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection_service.h"
#endif

#if !defined(OS_ANDROID)
#include "content/public/browser/plugin_service.h"
#endif

using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::WithArg;
using ::testing::_;
using content::DownloadItem;
using safe_browsing::DownloadFileType;

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  ~MockWebContentsDelegate() override {}
};

// Google Mock action that posts a task to the current message loop that invokes
// the first argument of the mocked method as a callback. Said argument must be
// a base::Callback<void(ParamType)>. |result| must be of |ParamType| and is
// bound as that parameter.
// Example:
//   class FooClass {
//    public:
//     virtual void Foo(base::Callback<void(bool)> callback);
//   };
//   ...
//   EXPECT_CALL(mock_fooclass_instance, Foo(callback))
//     .WillOnce(ScheduleCallback(false));
ACTION_P(ScheduleCallback, result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(arg0, result));
}

// Similar to ScheduleCallback, but binds 2 arguments.
ACTION_P2(ScheduleCallback2, result0, result1) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg0, result0, result1));
}

// Subclass of the ChromeDownloadManagerDelegate that uses a mock
// DownloadProtectionService.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    ON_CALL(*this, MockCheckDownloadUrl(_, _))
        .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*this, GetDownloadProtectionService())
        .WillByDefault(Return(nullptr));
  }

  ~TestChromeDownloadManagerDelegate() override {}

  void NotifyExtensions(content::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        const NotifyExtensionsCallback& callback) override {
    callback.Run(base::FilePath(),
                 DownloadPathReservationTracker::UNIQUIFY);
  }

  void ReserveVirtualPath(
      content::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      const DownloadPathReservationTracker::ReservedPathCallback& callback)
      override {
    // Pretend the path reservation succeeded without any change to
    // |target_path|.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, virtual_path, true));
  }

  void PromptUserForDownloadPath(
      DownloadItem* download,
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback)
      override {
    base::FilePath return_path = MockPromptUserForDownloadPath(download,
                                                               suggested_path,
                                                               callback);
    callback.Run(return_path);
  }

  void CheckDownloadUrl(DownloadItem* download,
                        const base::FilePath& virtual_path,
                        const CheckDownloadUrlCallback& callback) override {
    callback.Run(MockCheckDownloadUrl(download, virtual_path));
  }

  MOCK_METHOD0(GetDownloadProtectionService,
               safe_browsing::DownloadProtectionService*());

  MOCK_METHOD3(
      MockPromptUserForDownloadPath,
      base::FilePath(
          DownloadItem*,
          const base::FilePath&,
          const DownloadTargetDeterminerDelegate::FileSelectedCallback&));

  MOCK_METHOD2(MockCheckDownloadUrl,
               content::DownloadDangerType(DownloadItem*,
                                           const base::FilePath&));
};

class ChromeDownloadManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeDownloadManagerDelegateTest();

  // ::testing::Test
  void SetUp() override;
  void TearDown() override;

  // Verifies and clears test expectations for |delegate_| and
  // |download_manager_|.
  void VerifyAndClearExpectations();

  // Creates MockDownloadItem and sets up default expectations.
  std::unique_ptr<content::MockDownloadItem> CreateActiveDownloadItem(
      int32_t id);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  base::FilePath GetPathInDownloadDir(const char* path);

  // Set the kDownloadDefaultDirectory user preference to |path|.
  void SetDefaultDownloadPath(const base::FilePath& path);

  void DetermineDownloadTarget(DownloadItem* download,
                               DownloadTargetInfo* result);

  // Invokes ChromeDownloadManagerDelegate::CheckForFileExistence and waits for
  // the asynchronous callback. The result passed into
  // content::CheckForFileExistenceCallback is the return value from this
  // method.
  bool CheckForFileExistence(DownloadItem* download);

  const base::FilePath& default_download_path() const;
  TestChromeDownloadManagerDelegate* delegate();
  content::MockDownloadManager* download_manager();
  DownloadPrefs* download_prefs();

 private:
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  base::ScopedTempDir test_download_dir_;
  std::unique_ptr<content::MockDownloadManager> download_manager_;
  std::unique_ptr<TestChromeDownloadManagerDelegate> delegate_;
  MockWebContentsDelegate web_contents_delegate_;
};

ChromeDownloadManagerDelegateTest::ChromeDownloadManagerDelegateTest()
    : download_manager_(new ::testing::NiceMock<content::MockDownloadManager>) {
}

void ChromeDownloadManagerDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  CHECK(profile());
  delegate_.reset(new TestChromeDownloadManagerDelegate(profile()));
  delegate_->SetDownloadManager(download_manager_.get());
  pref_service_ = profile()->GetTestingPrefService();
  web_contents()->SetDelegate(&web_contents_delegate_);

  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  SetDefaultDownloadPath(test_download_dir_.GetPath());
}

void ChromeDownloadManagerDelegateTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  delegate_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
}

void ChromeDownloadManagerDelegateTest::VerifyAndClearExpectations() {
  ::testing::Mock::VerifyAndClearExpectations(delegate_.get());
}

std::unique_ptr<content::MockDownloadItem>
ChromeDownloadManagerDelegateTest::CreateActiveDownloadItem(int32_t id) {
  std::unique_ptr<content::MockDownloadItem> item(
      new ::testing::NiceMock<content::MockDownloadItem>());
  ON_CALL(*item, GetBrowserContext())
      .WillByDefault(Return(profile()));
  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetForcedFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetHash())
      .WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetId())
      .WillByDefault(Return(id));
  ON_CALL(*item, GetLastReason())
      .WillByDefault(Return(content::DOWNLOAD_INTERRUPT_REASON_NONE));
  ON_CALL(*item, GetReferrerUrl())
      .WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetState())
      .WillByDefault(Return(DownloadItem::IN_PROGRESS));
  ON_CALL(*item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(ui::PAGE_TRANSITION_LINK));
  ON_CALL(*item, GetWebContents())
      .WillByDefault(Return(web_contents()));
  ON_CALL(*item, HasUserGesture())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsDangerous())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  EXPECT_CALL(*download_manager_, GetDownload(id))
      .WillRepeatedly(Return(item.get()));
  return item;
}

base::FilePath ChromeDownloadManagerDelegateTest::GetPathInDownloadDir(
    const char* relative_path) {
  base::FilePath full_path =
      test_download_dir_.GetPath().AppendASCII(relative_path);
  return full_path.NormalizePathSeparators();
}

void ChromeDownloadManagerDelegateTest::SetDefaultDownloadPath(
    const base::FilePath& path) {
  pref_service_->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service_->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}

void StoreDownloadTargetInfo(const base::Closure& closure,
                         DownloadTargetInfo* target_info,
                         const base::FilePath& target_path,
                         DownloadItem::TargetDisposition target_disposition,
                         content::DownloadDangerType danger_type,
                         const base::FilePath& intermediate_path) {
  target_info->target_path = target_path;
  target_info->target_disposition = target_disposition;
  target_info->danger_type = danger_type;
  target_info->intermediate_path = intermediate_path;
  closure.Run();
}

void ChromeDownloadManagerDelegateTest::DetermineDownloadTarget(
    DownloadItem* download_item,
    DownloadTargetInfo* result) {
  base::RunLoop loop_runner;
  delegate()->DetermineDownloadTarget(
      download_item,
      base::Bind(&StoreDownloadTargetInfo, loop_runner.QuitClosure(), result));
  loop_runner.Run();
}

void StoreBoolAndRunClosure(const base::Closure& closure,
                            bool* result_storage,
                            bool result) {
  *result_storage = result;
  closure.Run();
}

bool ChromeDownloadManagerDelegateTest::CheckForFileExistence(
    DownloadItem* download_item) {
  base::RunLoop loop_runner;
  bool result = false;
  delegate()->CheckForFileExistence(
      download_item,
      base::Bind(&StoreBoolAndRunClosure, loop_runner.QuitClosure(), &result));
  loop_runner.Run();
  return result;
}

const base::FilePath& ChromeDownloadManagerDelegateTest::default_download_path()
    const {
  return test_download_dir_.GetPath();
}

TestChromeDownloadManagerDelegate*
    ChromeDownloadManagerDelegateTest::delegate() {
  return delegate_.get();
}

content::MockDownloadManager*
    ChromeDownloadManagerDelegateTest::download_manager() {
  return download_manager_.get();
}

DownloadPrefs* ChromeDownloadManagerDelegateTest::download_prefs() {
  return delegate_->download_prefs();
}

}  // namespace

// There is no "save as" context menu option on Android.
#if !defined(OS_ANDROID)
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_LastSavePath) {
  GURL download_url("http://example.com/foo.txt");

  std::unique_ptr<content::MockDownloadItem> save_as_download =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*save_as_download, GetURL())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*save_as_download, GetTargetDisposition())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));

  std::unique_ptr<content::MockDownloadItem> automatic_download =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*automatic_download, GetURL())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*automatic_download, GetTargetDisposition())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));

  {
    // When the prompt is displayed for the first download, the user selects a
    // path in a different directory.
    DownloadTargetInfo result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    base::FilePath user_selected_path(GetPathInDownloadDir("bar/baz.txt"));
    EXPECT_CALL(*delegate(),
                MockPromptUserForDownloadPath(save_as_download.get(),
                                              expected_prompt_path, _))
        .WillOnce(Return(user_selected_path));
    DetermineDownloadTarget(save_as_download.get(), &result);
    EXPECT_EQ(user_selected_path, result.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the second download is the user selected directroy
    // from the previous download.
    DownloadTargetInfo result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("bar/foo.txt"));
    EXPECT_CALL(*delegate(),
                MockPromptUserForDownloadPath(save_as_download.get(),
                                              expected_prompt_path, _))
        .WillOnce(Return(base::FilePath()));
    DetermineDownloadTarget(save_as_download.get(), &result);
    VerifyAndClearExpectations();
  }

  {
    // Start an automatic download. This one should get the default download
    // path since the last download path only affects Save As downloads.
    DownloadTargetInfo result;
    base::FilePath expected_path(GetPathInDownloadDir("foo.txt"));
    DetermineDownloadTarget(automatic_download.get(), &result);
    EXPECT_EQ(expected_path, result.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the next download should be the default.
    download_prefs()->SetSaveFilePath(download_prefs()->DownloadPath());
    DownloadTargetInfo result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    EXPECT_CALL(*delegate(),
                MockPromptUserForDownloadPath(save_as_download.get(),
                                              expected_prompt_path, _))
        .WillOnce(Return(base::FilePath()));
    DetermineDownloadTarget(save_as_download.get(), &result);
    VerifyAndClearExpectations();
  }
}
#endif  // !defined(OS_ANDROID)

TEST_F(ChromeDownloadManagerDelegateTest, MaybeDangerousContent) {
#if !defined(OS_ANDROID)
  content::PluginService::GetInstance()->Init();
#endif

  GURL url("http://example.com/foo");

  std::unique_ptr<content::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(url));
  EXPECT_CALL(*download_item, GetTargetDisposition())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  EXPECT_CALL(*delegate(), MockCheckDownloadUrl(_, _))
      .WillRepeatedly(
          Return(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT));

  {
    const std::string kDangerousContentDisposition(
        "attachment; filename=\"foo.swf\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kDangerousContentDisposition));
    DownloadTargetInfo target_info;
    DetermineDownloadTarget(download_item.get(), &target_info);

    EXPECT_EQ(DownloadFileType::DANGEROUS,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }

  {
    const std::string kSafeContentDisposition(
        "attachment; filename=\"foo.txt\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kSafeContentDisposition));
    DownloadTargetInfo target_info;
    DetermineDownloadTarget(download_item.get(), &target_info);
    EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }

  {
    const std::string kModerateContentDisposition(
        "attachment; filename=\"foo.crx\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kModerateContentDisposition));
    DownloadTargetInfo target_info;
    DetermineDownloadTarget(download_item.get(), &target_info);
    EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }
}

TEST_F(ChromeDownloadManagerDelegateTest, CheckForFileExistence) {
  const char kData[] = "helloworld";
  const size_t kDataLength = sizeof(kData) - 1;
  base::FilePath existing_path = default_download_path().AppendASCII("foo");
  base::FilePath non_existent_path =
      default_download_path().AppendASCII("bar");
  base::WriteFile(existing_path, kData, kDataLength);

  std::unique_ptr<content::MockDownloadItem> download_item =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*download_item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(existing_path));
  EXPECT_TRUE(CheckForFileExistence(download_item.get()));

  download_item = CreateActiveDownloadItem(1);
  EXPECT_CALL(*download_item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(non_existent_path));
  EXPECT_FALSE(CheckForFileExistence(download_item.get()));
}

#if defined(FULL_SAFE_BROWSING)
namespace {

struct SafeBrowsingTestParameters {
  content::DownloadDangerType initial_danger_type;
  DownloadFileType::DangerLevel initial_danger_level;
  safe_browsing::DownloadProtectionService::DownloadCheckResult verdict;

  content::DownloadDangerType expected_danger_type;
};

class TestDownloadProtectionService
    : public safe_browsing::DownloadProtectionService {
 public:
  TestDownloadProtectionService() : DownloadProtectionService(nullptr) {}

  void CheckClientDownload(DownloadItem* download_item,
                           const CheckDownloadCallback& callback) override {
    callback.Run(MockCheckClientDownload());
  }
  MOCK_METHOD0(MockCheckClientDownload,
               safe_browsing::DownloadProtectionService::DownloadCheckResult());
};

class ChromeDownloadManagerDelegateTestWithSafeBrowsing
    : public ChromeDownloadManagerDelegateTest,
      public ::testing::WithParamInterface<SafeBrowsingTestParameters> {
 public:
  void SetUp() override;
  void TearDown() override;
  TestDownloadProtectionService* download_protection_service() {
    return test_download_protection_service_.get();
  }

 private:
  std::unique_ptr<TestDownloadProtectionService>
      test_download_protection_service_;
};

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::SetUp() {
  ChromeDownloadManagerDelegateTest::SetUp();
  test_download_protection_service_.reset(
      new ::testing::StrictMock<TestDownloadProtectionService>);
  ON_CALL(*delegate(), GetDownloadProtectionService())
      .WillByDefault(Return(test_download_protection_service_.get()));
}

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::TearDown() {
  test_download_protection_service_.reset();
  ChromeDownloadManagerDelegateTest::TearDown();
}

const SafeBrowsingTestParameters kSafeBrowsingTestCases[] = {
    // SAFE verdict for a safe file.
    {content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadProtectionService::SAFE,

     content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},

    // UNKNOWN verdict for a safe file.
    {content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadProtectionService::UNKNOWN,

     content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},

    // DANGEROUS verdict for a safe file.
    {content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadProtectionService::DANGEROUS,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},

    // UNCOMMON verdict for a safe file.
    {content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadProtectionService::UNCOMMON,

     content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT},

    // POTENTIALLY_UNWANTED verdict for a safe file.
    {content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadProtectionService::POTENTIALLY_UNWANTED,

     content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED},

    // SAFE verdict for a potentially dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadProtectionService::SAFE,

     content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},

    // UNKNOWN verdict for a potentially dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadProtectionService::UNKNOWN,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE},

    // DANGEROUS verdict for a potentially dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadProtectionService::DANGEROUS,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},

    // UNCOMMON verdict for a potentially dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadProtectionService::UNCOMMON,

     content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT},

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadProtectionService::POTENTIALLY_UNWANTED,

     content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED},

    // SAFE verdict for a dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadProtectionService::SAFE,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE},

    // UNKNOWN verdict for a dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadProtectionService::UNKNOWN,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE},

    // DANGEROUS verdict for a dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadProtectionService::DANGEROUS,

     content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},

    // UNCOMMON verdict for a dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadProtectionService::UNCOMMON,

     content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT},

    // POTENTIALLY_UNWANTED verdict for a dangerous file.
    {content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadProtectionService::POTENTIALLY_UNWANTED,

     content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED},
};

INSTANTIATE_TEST_CASE_P(_,
                        ChromeDownloadManagerDelegateTestWithSafeBrowsing,
                        ::testing::ValuesIn(kSafeBrowsingTestCases));

}  // namespace

TEST_P(ChromeDownloadManagerDelegateTestWithSafeBrowsing, CheckClientDownload) {
  const SafeBrowsingTestParameters& kParameters = GetParam();

  std::unique_ptr<content::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*delegate(), GetDownloadProtectionService());
  EXPECT_CALL(*download_protection_service(), MockCheckClientDownload())
      .WillOnce(Return(kParameters.verdict));
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(kParameters.initial_danger_type));

  if (kParameters.initial_danger_level != DownloadFileType::NOT_DANGEROUS) {
    DownloadItemModel(download_item.get())
        .SetDangerLevel(kParameters.initial_danger_level);
  }

  if (kParameters.expected_danger_type !=
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    EXPECT_CALL(*download_item,
                OnContentCheckCompleted(kParameters.expected_danger_type));
  } else {
    EXPECT_CALL(*download_item, OnContentCheckCompleted(_)).Times(0);
  }

  base::RunLoop run_loop;
  ASSERT_FALSE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                  run_loop.QuitClosure()));
  run_loop.Run();
}

#endif  // FULL_SAFE_BROWSING
