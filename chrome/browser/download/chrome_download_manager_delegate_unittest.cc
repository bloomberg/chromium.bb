// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  virtual ~MockWebContentsDelegate() {}
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
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(arg0, result));
}

// Similar to ScheduleCallback, but binds 2 arguments.
ACTION_P2(ScheduleCallback2, result0, result1) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(arg0, result0, result1));
}

// Subclass of the ChromeDownloadManagerDelegate that uses a mock
// DownloadProtectionService.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
  }

  virtual ~TestChromeDownloadManagerDelegate() {}

  virtual safe_browsing::DownloadProtectionService*
      GetDownloadProtectionService() OVERRIDE {
    return NULL;
  }

  virtual void NotifyExtensions(
      content::DownloadItem* download,
      const base::FilePath& suggested_virtual_path,
      const NotifyExtensionsCallback& callback) OVERRIDE {
    callback.Run(base::FilePath(),
                 DownloadPathReservationTracker::UNIQUIFY);
  }

  virtual void ReserveVirtualPath(
      content::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      const DownloadPathReservationTracker::ReservedPathCallback& callback)
      OVERRIDE {
    // Pretend the path reservation succeeded without any change to
    // |target_path|.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, virtual_path, true));
  }

  virtual void PromptUserForDownloadPath(
      DownloadItem* download,
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback)
      OVERRIDE {
    base::FilePath return_path = MockPromptUserForDownloadPath(download,
                                                               suggested_path,
                                                               callback);
    callback.Run(return_path);
  }

  MOCK_METHOD3(
      MockPromptUserForDownloadPath,
      base::FilePath(
          content::DownloadItem*,
          const base::FilePath&,
          const DownloadTargetDeterminerDelegate::FileSelectedCallback&));
};

class ChromeDownloadManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeDownloadManagerDelegateTest();

  // ::testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Verifies and clears test expectations for |delegate_| and
  // |download_manager_|.
  void VerifyAndClearExpectations();

  // Creates MockDownloadItem and sets up default expectations.
  content::MockDownloadItem* CreateActiveDownloadItem(int32 id);

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
  TestingPrefServiceSyncable* pref_service_;
  base::ScopedTempDir test_download_dir_;
  scoped_ptr<content::MockDownloadManager> download_manager_;
  scoped_ptr<TestChromeDownloadManagerDelegate> delegate_;
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
  SetDefaultDownloadPath(test_download_dir_.path());
}

void ChromeDownloadManagerDelegateTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  delegate_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
}

void ChromeDownloadManagerDelegateTest::VerifyAndClearExpectations() {
  ::testing::Mock::VerifyAndClearExpectations(delegate_.get());
}

content::MockDownloadItem*
    ChromeDownloadManagerDelegateTest::CreateActiveDownloadItem(int32 id) {
  content::MockDownloadItem* item =
      new ::testing::NiceMock<content::MockDownloadItem>();
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
      .WillRepeatedly(Return(item));
  return item;
}

base::FilePath ChromeDownloadManagerDelegateTest::GetPathInDownloadDir(
    const char* relative_path) {
  base::FilePath full_path =
      test_download_dir_.path().AppendASCII(relative_path);
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
  return test_download_dir_.path();
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

TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_LastSavePath) {
  GURL download_url("http://example.com/foo.txt");

  scoped_ptr<content::MockDownloadItem> save_as_download(
      CreateActiveDownloadItem(0));
  EXPECT_CALL(*save_as_download, GetURL())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*save_as_download, GetTargetDisposition())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));

  scoped_ptr<content::MockDownloadItem> automatic_download(
      CreateActiveDownloadItem(1));
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

TEST_F(ChromeDownloadManagerDelegateTest, CheckForFileExistence) {
  const char kData[] = "helloworld";
  const size_t kDataLength = sizeof(kData) - 1;
  base::FilePath existing_path = default_download_path().AppendASCII("foo");
  base::FilePath non_existent_path =
      default_download_path().AppendASCII("bar");
  base::WriteFile(existing_path, kData, kDataLength);

  scoped_ptr<content::MockDownloadItem> download_item(
      CreateActiveDownloadItem(1));
  EXPECT_CALL(*download_item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(existing_path));
  EXPECT_TRUE(CheckForFileExistence(download_item.get()));

  download_item.reset(CreateActiveDownloadItem(1));
  EXPECT_CALL(*download_item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(non_existent_path));
  EXPECT_FALSE(CheckForFileExistence(download_item.get()));
}
