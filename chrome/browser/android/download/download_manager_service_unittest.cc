// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_manager_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using ::testing::_;

namespace content {
class BrowserContext;
class ByteStreamReader;
class DownloadManagerDelegate;
struct DownloadCreateInfo;
}

// Mock implementation of content::DownloadItem.
class MockDownloadItem : public content::DownloadItem {
 public:
  explicit MockDownloadItem(bool can_resume) : can_resume_(can_resume) {}
  ~MockDownloadItem() override {}
  bool CanResume() const override { return can_resume_; }

  MOCK_METHOD1(AddObserver, void(content::DownloadItem::Observer*));
  MOCK_METHOD1(RemoveObserver, void(content::DownloadItem::Observer*));
  MOCK_METHOD0(UpdateObservers, void());
  MOCK_METHOD0(ValidateDangerousDownload, void());
  MOCK_METHOD1(StealDangerousDownload,
               void(const content::DownloadItem::AcquireFileCallback&));
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Resume, void());
  MOCK_METHOD1(Cancel, void(bool));
  MOCK_METHOD0(Remove, void());
  MOCK_METHOD0(OpenDownload, void());
  MOCK_METHOD0(ShowDownloadInShell, void());
  MOCK_CONST_METHOD0(GetId, uint32_t());
  MOCK_CONST_METHOD0(GetState, content::DownloadItem::DownloadState());
  MOCK_CONST_METHOD0(GetLastReason, content::DownloadInterruptReason());
  MOCK_CONST_METHOD0(IsPaused, bool());
  MOCK_CONST_METHOD0(IsTemporary, bool());
  MOCK_CONST_METHOD0(IsDone, bool());
  MOCK_CONST_METHOD0(GetURL, const GURL&());
  MOCK_CONST_METHOD0(GetUrlChain, std::vector<GURL>&());
  MOCK_CONST_METHOD0(GetOriginalUrl, const GURL&());
  MOCK_CONST_METHOD0(GetReferrerUrl, const GURL&());
  MOCK_CONST_METHOD0(GetTabUrl, const GURL&());
  MOCK_CONST_METHOD0(GetTabReferrerUrl, const GURL&());
  MOCK_CONST_METHOD0(GetSuggestedFilename, std::string());
  MOCK_CONST_METHOD0(GetContentDisposition, std::string());
  MOCK_CONST_METHOD0(GetMimeType, std::string());
  MOCK_CONST_METHOD0(GetOriginalMimeType, std::string());
  MOCK_CONST_METHOD0(GetRemoteAddress, std::string());
  MOCK_CONST_METHOD0(HasUserGesture, bool());
  MOCK_CONST_METHOD0(GetTransitionType, ui::PageTransition());
  MOCK_CONST_METHOD0(GetLastModifiedTime, const std::string&());
  MOCK_CONST_METHOD0(GetETag, const std::string&());
  MOCK_CONST_METHOD0(IsSavePackageDownload, bool());
  MOCK_CONST_METHOD0(GetFullPath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetTargetFilePath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetForcedFilePath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetFileNameToReportUser, base::FilePath());
  MOCK_CONST_METHOD0(GetTargetDisposition,
                     content::DownloadItem::TargetDisposition());
  MOCK_CONST_METHOD0(GetHash, const std::string&());
  MOCK_CONST_METHOD0(GetHashState, const std::string&());
  MOCK_CONST_METHOD0(GetFileExternallyRemoved, bool());
  MOCK_METHOD1(DeleteFile, void(const base::Callback<void(bool)>&));
  MOCK_CONST_METHOD0(IsDangerous, bool());
  MOCK_CONST_METHOD0(GetDangerType, content::DownloadDangerType());
  MOCK_CONST_METHOD1(TimeRemaining, bool(base::TimeDelta*));
  MOCK_CONST_METHOD0(CurrentSpeed, int64_t());
  MOCK_CONST_METHOD0(PercentComplete, int());
  MOCK_CONST_METHOD0(AllDataSaved, bool());
  MOCK_CONST_METHOD0(GetTotalBytes, int64_t());
  MOCK_CONST_METHOD0(GetReceivedBytes, int64_t());
  MOCK_CONST_METHOD0(GetStartTime, base::Time());
  MOCK_CONST_METHOD0(GetEndTime, base::Time());
  MOCK_METHOD0(CanShowInFolder, bool());
  MOCK_METHOD0(CanOpenDownload, bool());
  MOCK_METHOD0(ShouldOpenFileBasedOnExtension, bool());
  MOCK_CONST_METHOD0(GetOpenWhenComplete, bool());
  MOCK_METHOD0(GetAutoOpened, bool());
  MOCK_CONST_METHOD0(GetOpened, bool());
  MOCK_CONST_METHOD0(GetBrowserContext, content::BrowserContext*());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD1(OnContentCheckCompleted, void(content::DownloadDangerType));
  MOCK_METHOD1(SetOpenWhenComplete, void(bool));
  MOCK_METHOD1(SetIsTemporary, void(bool));
  MOCK_METHOD1(SetOpened, void(bool));
  MOCK_METHOD1(SetDisplayName, void(const base::FilePath&));
  MOCK_CONST_METHOD1(DebugString, std::string(bool));

 private:
  bool can_resume_;
};

// Mock implementation of content::DownloadManager.
class MockDownloadManager : public content::DownloadManager {
 public:
  MockDownloadManager() {}
  ~MockDownloadManager() override {}

  MOCK_METHOD1(SetDelegate, void(content::DownloadManagerDelegate*));
  MOCK_CONST_METHOD0(GetDelegate, content::DownloadManagerDelegate*());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(GetAllDownloads, void(DownloadVector*));
  MOCK_METHOD3(RemoveDownloadsByOriginAndTime,
               int(const url::Origin&, base::Time, base::Time));
  MOCK_METHOD2(RemoveDownloadsBetween, int(base::Time, base::Time));
  MOCK_METHOD1(RemoveDownloads, int(base::Time));
  MOCK_METHOD0(RemoveAllDownloads, int());
  void DownloadUrl(scoped_ptr<content::DownloadUrlParameters>) override {}
  MOCK_METHOD1(AddObserver, void(content::DownloadManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(content::DownloadManager::Observer*));
  MOCK_CONST_METHOD0(InProgressCount, int());
  MOCK_CONST_METHOD0(NonMaliciousInProgressCount, int());
  MOCK_CONST_METHOD0(GetBrowserContext, content::BrowserContext*());
  MOCK_METHOD0(CheckForHistoryFilesRemoval, void());
  void StartDownload(
      scoped_ptr<content::DownloadCreateInfo>,
      scoped_ptr<content::ByteStreamReader>,
      const content::DownloadUrlParameters::OnStartedCallback&) override {}
  content::DownloadItem* CreateDownloadItem(
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      content::DownloadItem::DownloadState state,
      content::DownloadDangerType danger_type,
      content::DownloadInterruptReason interrupt_reason,
      bool opened) override {
    return nullptr;
  }
  content::DownloadItem* GetDownload(uint32_t id) override {
    return download_item_.get();
  }
  void SetDownload(MockDownloadItem* item) { download_item_.reset(item); }

 private:
  scoped_ptr<MockDownloadItem> download_item_;
};

class DownloadManagerServiceTest : public testing::Test {
 public:
  DownloadManagerServiceTest()
      : service_(
            new DownloadManagerService(base::android::AttachCurrentThread(),
                                       nullptr,
                                       &manager_)),
        finished_(false),
        success_(false) {}

  void OnResumptionDone(bool success) {
    finished_ = true;
    success_ = success;
  }

  void StartDownload(int download_id) {
    JNIEnv* env = base::android::AttachCurrentThread();
    service_->set_resume_callback_for_testing(base::Bind(
        &DownloadManagerServiceTest::OnResumptionDone, base::Unretained(this)));
    service_->ResumeDownload(
        env, nullptr, download_id,
        base::android::ConvertUTF8ToJavaString(env, "test").obj());
    while (!finished_)
      message_loop_.RunUntilIdle();
  }

  void CreateDownloadItem(bool can_resume) {
    manager_.SetDownload(new MockDownloadItem(can_resume));
  }

 protected:
  base::MessageLoop message_loop_;
  MockDownloadManager manager_;
  DownloadManagerService* service_;
  bool finished_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerServiceTest);
};

// Test that resumption will fail if no download item is found before times out.
TEST_F(DownloadManagerServiceTest, ResumptionTimeOut) {
  StartDownload(1);
  EXPECT_FALSE(success_);
}

// Test that resumption succeeds if the download item is found and can be
// resumed.
TEST_F(DownloadManagerServiceTest, ResumptionWithResumableItem) {
  CreateDownloadItem(true);
  StartDownload(1);
  EXPECT_TRUE(success_);
}

// Test that resumption fails if the target download item is not resumable.
TEST_F(DownloadManagerServiceTest, ResumptionWithNonResumableItem) {
  CreateDownloadItem(false);
  StartDownload(1);
  EXPECT_FALSE(success_);
}
