// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/download/mock_download_manager.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

class DownloadManagerTest : public testing::Test {
 public:
  DownloadManagerTest()
      : profile_(new TestingProfile()),
        download_manager_(new MockDownloadManager(&download_status_updater_)),
        ui_thread_(BrowserThread::UI, &message_loop_) {
    download_manager_->Init(profile_.get());
  }

  ~DownloadManagerTest() {
    download_manager_->Shutdown();
    // profile_ must outlive download_manager_, so we explicitly delete
    // download_manager_ first.
    download_manager_ = NULL;
    profile_.reset(NULL);
    message_loop_.RunAllPending();
  }

  void AddDownloadToFileManager(int id, DownloadFile* download) {
    file_manager()->downloads_[id] = download;
  }

 protected:
  DownloadStatusUpdater download_status_updater_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<DownloadFileManager> file_manager_;
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;

  DownloadFileManager* file_manager() {
    if (!file_manager_) {
      file_manager_ = new DownloadFileManager(NULL);
      download_manager_->file_manager_ = file_manager_;
    }
    return file_manager_;
  }

  // Make sure download item |id| was set with correct safety state for
  // given |is_dangerous_file| and |is_dangerous_url|.
  bool VerifySafetyState(bool is_dangerous_file,
                         bool is_dangerous_url,
                         int id) {
    DownloadItem::SafetyState safety_state =
        download_manager_->GetDownloadItem(id)->safety_state();
    return (is_dangerous_file || is_dangerous_url) ?
        safety_state != DownloadItem::SAFE : safety_state == DownloadItem::SAFE;
  }

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTest);
};

namespace {

const struct {
  const char* url;
  const char* mime_type;
  bool save_as;
  bool prompt_for_download;
  bool expected_save_as;
} kStartDownloadCases[] = {
  { "http://www.foo.com/dont-open.html",
    "text/html",
    false,
    false,
    false, },
  { "http://www.foo.com/save-as.html",
    "text/html",
    true,
    false,
    true, },
  { "http://www.foo.com/always-prompt.html",
    "text/html",
    false,
    true,
    true, },
  { "http://www.foo.com/user-script-text-html-mimetype.user.js",
    "text/html",
    false,
    false,
    false, },
  { "http://www.foo.com/extensionless-extension",
    "application/x-chrome-extension",
    true,
    false,
    true, },
  { "http://www.foo.com/save-as.pdf",
    "application/pdf",
    true,
    false,
    true, },
  { "http://www.foo.com/sometimes_prompt.pdf",
    "application/pdf",
    false,
    true,
    false, },
  { "http://www.foo.com/always_prompt.jar",
    "application/jar",
    false,
    true,
    true, },
};

}  // namespace

TEST_F(DownloadManagerTest, StartDownload) {
  BrowserThread io_thread(BrowserThread::IO, &message_loop_);
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetFilePath(prefs::kDownloadDefaultDirectory, FilePath());
  download_manager_->download_prefs()->EnableAutoOpenBasedOnExtension(
      FilePath(FILE_PATH_LITERAL("example.pdf")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStartDownloadCases); ++i) {
    prefs->SetBoolean(prefs::kPromptForDownload,
                      kStartDownloadCases[i].prompt_for_download);

    DownloadCreateInfo info;
    info.prompt_user_for_save_location = kStartDownloadCases[i].save_as;
    info.url = GURL(kStartDownloadCases[i].url);
    info.mime_type = kStartDownloadCases[i].mime_type;

    download_manager_->StartDownload(&info);
    message_loop_.RunAllPending();

    EXPECT_EQ(kStartDownloadCases[i].expected_save_as,
        info.prompt_user_for_save_location);
  }
}

namespace {

const struct {
  FilePath::StringType suggested_path;
  bool is_dangerous_file;
  bool is_dangerous_url;
  bool finish_before_rename;
  int expected_rename_count;
} kDownloadRenameCases[] = {
  // Safe download, download finishes BEFORE file name determined.
  // Renamed twice (linear path through UI).  Crdownload file does not need
  // to be deleted.
  { FILE_PATH_LITERAL("foo.zip"),
    false, false, true, 2, },
  // Dangerous download (file is dangerous or download URL is not safe or both),
  // download finishes BEFORE file name determined. Needs to be renamed only
  // once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    true, false, true, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    false, true, true, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    true, true, true, 1, },
  // Safe download, download finishes AFTER file name determined.
  // Needs to be renamed twice.
  { FILE_PATH_LITERAL("foo.zip"),
    false, false, false, 2, },
  // Dangerous download, download finishes AFTER file name determined.
  // Needs to be renamed only once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    true, false, false, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    false, true, false, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    true, true, false, 1, },
};

class MockDownloadFile : public DownloadFile {
 public:
  explicit MockDownloadFile(DownloadCreateInfo* info)
      : DownloadFile(info, NULL), renamed_count_(0) { }
  virtual ~MockDownloadFile() { Destructed(); }
  MOCK_METHOD1(Rename, bool(const FilePath&));
  MOCK_METHOD0(Destructed, void());

  bool TestMultipleRename(
      int expected_count, const FilePath& expected,
      const FilePath& path) {
    ++renamed_count_;
    EXPECT_EQ(expected_count, renamed_count_);
    EXPECT_EQ(expected.value(), path.value());
    return true;
  }

 private:
  int renamed_count_;
};

}  // namespace

TEST_F(DownloadManagerTest, DownloadRenameTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  BrowserThread file_thread(BrowserThread::FILE, &message_loop_);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDownloadRenameCases); ++i) {
    // |info| will be destroyed in download_manager_.
    DownloadCreateInfo* info(new DownloadCreateInfo);
    info->download_id = static_cast<int>(i);
    info->prompt_user_for_save_location = false;
    info->is_dangerous_file = kDownloadRenameCases[i].is_dangerous_file;
    info->is_dangerous_url = kDownloadRenameCases[i].is_dangerous_url;
    FilePath new_path(kDownloadRenameCases[i].suggested_path);

    MockDownloadFile* download(new MockDownloadFile(info));
    AddDownloadToFileManager(info->download_id, download);

    // |download| is owned by DownloadFileManager.
    ::testing::Mock::AllowLeak(download);
    EXPECT_CALL(*download, Destructed()).Times(1);

    if (kDownloadRenameCases[i].expected_rename_count == 1) {
      EXPECT_CALL(*download, Rename(new_path)).WillOnce(Return(true));
    } else {
      ASSERT_EQ(2, kDownloadRenameCases[i].expected_rename_count);
      FilePath crdownload(download_util::GetCrDownloadPath(new_path));
      EXPECT_CALL(*download, Rename(_))
          .WillOnce(testing::WithArgs<0>(Invoke(CreateFunctor(
              download, &MockDownloadFile::TestMultipleRename,
              1, crdownload))))
          .WillOnce(testing::WithArgs<0>(Invoke(CreateFunctor(
              download, &MockDownloadFile::TestMultipleRename,
              2, new_path))));
    }
    download_manager_->CreateDownloadItem(info);

    if (kDownloadRenameCases[i].finish_before_rename) {
      download_manager_->OnAllDataSaved(i, 1024, std::string("fake_hash"));
      download_manager_->FileSelected(new_path, i, info);
    } else {
      download_manager_->FileSelected(new_path, i, info);
      download_manager_->OnAllDataSaved(i, 1024, std::string("fake_hash"));
    }

    message_loop_.RunAllPending();
    EXPECT_TRUE(VerifySafetyState(kDownloadRenameCases[i].is_dangerous_file,
                                  kDownloadRenameCases[i].is_dangerous_url,
                                  i));
  }
}
