// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <set>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
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
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : profile_(new TestingProfile()),
        download_manager_(new MockDownloadManager(&download_status_updater_)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
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

  void AddDownloadToFileManager(int id, DownloadFile* download_file) {
    file_manager()->downloads_[id] = download_file;
  }

  void OnAllDataSaved(int32 download_id, int64 size, const std::string& hash) {
    download_manager_->OnAllDataSaved(download_id, size, hash);
  }

  void FileSelected(const FilePath& path, int index, void* params) {
    download_manager_->FileSelected(path, index, params);
  }

  void AttachDownloadItem(DownloadCreateInfo* info) {
    download_manager_->AttachDownloadItem(info);
  }

  void OnDownloadError(int32 download_id, int64 size, int os_error) {
    download_manager_->OnDownloadError(download_id, size, os_error);
  }

  // Get the download item with ID |id|.
  DownloadItem* GetActiveDownloadItem(int32 id) {
    if (ContainsKey(download_manager_->active_downloads_, id))
      return download_manager_->active_downloads_[id];
    return NULL;
  }

 protected:
  DownloadStatusUpdater download_status_updater_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<DownloadFileManager> file_manager_;
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

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

const char* DownloadManagerTest::kTestData = "a;sdlfalsdfjalsdkfjad";
const size_t DownloadManagerTest::kTestDataLen =
    strlen(DownloadManagerTest::kTestData);

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
  MockDownloadFile(DownloadCreateInfo* info, DownloadManager* manager)
      : DownloadFile(info, manager), renamed_count_(0) { }
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

// This is an observer that records what download IDs have opened a select
// file dialog.
class SelectFileObserver : public DownloadManager::Observer {
 public:
  explicit SelectFileObserver(DownloadManager* download_manager)
      : download_manager_(download_manager) {
    DCHECK(download_manager_.get());
    download_manager_->AddObserver(this);
  }

  ~SelectFileObserver() {
    download_manager_->RemoveObserver(this);
  }

  // Downloadmanager::Observer functions.
  virtual void ModelChanged() {}
  virtual void ManagerGoingDown() {}
  virtual void SelectFileDialogDisplayed(int32 id) {
    file_dialog_ids_.insert(id);
  }

  bool ShowedFileDialogForId(int32 id) {
    return file_dialog_ids_.find(id) != file_dialog_ids_.end();
  }

 private:
  std::set<int32> file_dialog_ids_;
  scoped_refptr<DownloadManager> download_manager_;
};

// This observer tracks the progress of |DownloadItem|s.
class ItemObserver : public DownloadItem::Observer {
 public:
  explicit ItemObserver(DownloadItem* tracked)
      : tracked_(tracked), states_hit_(0),
        was_updated_(false), was_opened_(false) {
    DCHECK(tracked_);
    tracked_->AddObserver(this);
    // Record the initial state.
    OnDownloadUpdated(tracked_);
  }
  ~ItemObserver() {
    tracked_->RemoveObserver(this);
  }

  bool hit_state(int state) const {
    return (1 << state) & states_hit_;
  }
  bool was_updated() const { return was_updated_; }
  bool was_opened() const { return was_opened_; }

 private:
  // DownloadItem::Observer methods
  virtual void OnDownloadUpdated(DownloadItem* download) {
    DCHECK_EQ(tracked_, download);
    states_hit_ |= (1 << download->state());
    was_updated_ = true;
  }
  virtual void OnDownloadOpened(DownloadItem* download) {
    DCHECK_EQ(tracked_, download);
    states_hit_ |= (1 << download->state());
    was_opened_ = true;
  }

  DownloadItem* tracked_;
  int states_hit_;
  bool was_updated_;
  bool was_opened_;
};

}  // namespace

#if !defined(OS_CHROMEOS)

TEST_F(DownloadManagerTest, StartDownload) {
  BrowserThread io_thread(BrowserThread::IO, &message_loop_);
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetFilePath(prefs::kDownloadDefaultDirectory, FilePath());
  download_manager_->download_prefs()->EnableAutoOpenBasedOnExtension(
      FilePath(FILE_PATH_LITERAL("example.pdf")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStartDownloadCases); ++i) {
    prefs->SetBoolean(prefs::kPromptForDownload,
                      kStartDownloadCases[i].prompt_for_download);

    SelectFileObserver observer(download_manager_);
    DownloadCreateInfo* info = new DownloadCreateInfo;
    info->download_id = static_cast<int>(i);
    info->prompt_user_for_save_location = kStartDownloadCases[i].save_as;
    info->url_chain.push_back(GURL(kStartDownloadCases[i].url));
    info->mime_type = kStartDownloadCases[i].mime_type;
    download_manager_->CreateDownloadItem(info);

    DownloadFile* download_file(new DownloadFile(info, download_manager_));
    AddDownloadToFileManager(info->download_id, download_file);
    download_file->Initialize(false);
    download_manager_->StartDownload(info);
    message_loop_.RunAllPending();

    // NOTE: At this point, |AttachDownloadItem| will have been run if we don't
    // need to prompt the user, so |info| could have been destructed.
    // This means that we can't check any of its values.
    // However, SelectFileObserver will have recorded any attempt to open the
    // select file dialog.
    EXPECT_EQ(kStartDownloadCases[i].expected_save_as,
              observer.ShowedFileDialogForId(i));

    // If the Save As dialog pops up, it never reached
    // DownloadManager::AttachDownloadItem(), and never deleted info or
    // completed.  This cleans up info.
    // Note that DownloadManager::FileSelectionCanceled() is never called.
    if (observer.ShowedFileDialogForId(i)) {
      delete info;
    }
  }
}

#endif // !defined(OS_CHROMEOS)

TEST_F(DownloadManagerTest, DownloadRenameTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDownloadRenameCases); ++i) {
    // |info| will be destroyed in download_manager_.
    DownloadCreateInfo* info(new DownloadCreateInfo);
    info->download_id = static_cast<int>(i);
    info->prompt_user_for_save_location = false;
    info->url_chain.push_back(GURL());
    info->is_dangerous_file = kDownloadRenameCases[i].is_dangerous_file;
    info->is_dangerous_url = kDownloadRenameCases[i].is_dangerous_url;
    FilePath new_path(kDownloadRenameCases[i].suggested_path);

    MockDownloadFile* download_file(
        new MockDownloadFile(info, download_manager_));
    AddDownloadToFileManager(info->download_id, download_file);

    // |download_file| is owned by DownloadFileManager.
    ::testing::Mock::AllowLeak(download_file);
    EXPECT_CALL(*download_file, Destructed()).Times(1);

    if (kDownloadRenameCases[i].expected_rename_count == 1) {
      EXPECT_CALL(*download_file, Rename(new_path)).WillOnce(Return(true));
    } else {
      ASSERT_EQ(2, kDownloadRenameCases[i].expected_rename_count);
      FilePath crdownload(download_util::GetCrDownloadPath(new_path));
      EXPECT_CALL(*download_file, Rename(_))
          .WillOnce(testing::WithArgs<0>(Invoke(CreateFunctor(
              download_file, &MockDownloadFile::TestMultipleRename,
              1, crdownload))))
          .WillOnce(testing::WithArgs<0>(Invoke(CreateFunctor(
              download_file, &MockDownloadFile::TestMultipleRename,
              2, new_path))));
    }
    download_manager_->CreateDownloadItem(info);

    if (kDownloadRenameCases[i].finish_before_rename) {
      OnAllDataSaved(i, 1024, std::string("fake_hash"));
      message_loop_.RunAllPending();
      FileSelected(new_path, i, info);
    } else {
      FileSelected(new_path, i, info);
      message_loop_.RunAllPending();
      OnAllDataSaved(i, 1024, std::string("fake_hash"));
    }

    message_loop_.RunAllPending();
    EXPECT_TRUE(VerifySafetyState(kDownloadRenameCases[i].is_dangerous_file,
                                  kDownloadRenameCases[i].is_dangerous_url,
                                  i));
  }
}

TEST_F(DownloadManagerTest, DownloadInterruptTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  // |info| will be destroyed in download_manager_.
  DownloadCreateInfo* info(new DownloadCreateInfo);
  info->download_id = static_cast<int>(0);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->is_dangerous_file = false;
  info->is_dangerous_url = false;
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(download_util::GetCrDownloadPath(new_path));

  MockDownloadFile* download_file(
      new MockDownloadFile(info, download_manager_));
  AddDownloadToFileManager(info->download_id, download_file);

  // |download_file| is owned by DownloadFileManager.
  ::testing::Mock::AllowLeak(download_file);
  EXPECT_CALL(*download_file, Destructed()).Times(1);

  EXPECT_CALL(*download_file, Rename(cr_path)).WillOnce(Return(true));

  download_manager_->CreateDownloadItem(info);

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->state());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  info->path = new_path;
  AttachDownloadItem(info);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  OnDownloadError(0, 1024, -6);
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(0) == NULL);
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->state());
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());

  download->Cancel(true);

  EXPECT_EQ(DownloadItem::INTERRUPTED, download->state());
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
}

TEST_F(DownloadManagerTest, DownloadCancelTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  // |info| will be destroyed in download_manager_.
  DownloadCreateInfo* info(new DownloadCreateInfo);
  info->download_id = static_cast<int>(0);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->is_dangerous_file = false;
  info->is_dangerous_url = false;
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(download_util::GetCrDownloadPath(new_path));

  MockDownloadFile* download_file(
      new MockDownloadFile(info, download_manager_));
  AddDownloadToFileManager(info->download_id, download_file);

  // |download_file| is owned by DownloadFileManager.
  ::testing::Mock::AllowLeak(download_file);
  EXPECT_CALL(*download_file, Destructed()).Times(1);

  EXPECT_CALL(*download_file, Rename(cr_path)).WillOnce(Return(true));

  download_manager_->CreateDownloadItem(info);

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->state());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  info->path = new_path;
  AttachDownloadItem(info);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  download->Cancel(false);
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());

  EXPECT_FALSE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));
}

TEST_F(DownloadManagerTest, DownloadOverwriteTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  // Create a temporary directory.
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // File names we're using.
  const FilePath new_path(temp_dir_.path().AppendASCII("foo.txt"));
  const FilePath cr_path(download_util::GetCrDownloadPath(new_path));
  EXPECT_FALSE(file_util::PathExists(new_path));

  // Create the file that we will overwrite.  Will be automatically cleaned
  // up when temp_dir_ is destroyed.
  FILE* fp = file_util::OpenFile(new_path, "w");
  file_util::CloseFile(fp);
  EXPECT_TRUE(file_util::PathExists(new_path));

  // Construct the unique file name that normally would be created, but
  // which we will override.
  int uniquifier = download_util::GetUniquePathNumber(new_path);
  FilePath unique_new_path = new_path;
  EXPECT_NE(0, uniquifier);
  download_util::AppendNumberToPath(&unique_new_path, uniquifier);

  // |info| will be destroyed in download_manager_.
  DownloadCreateInfo* info(new DownloadCreateInfo);
  info->download_id = static_cast<int>(0);
  info->prompt_user_for_save_location = true;
  info->url_chain.push_back(GURL());
  info->is_dangerous_file = false;
  info->is_dangerous_url = false;

  download_manager_->CreateDownloadItem(info);

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->state());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFile(info, download_manager_));
  download_file->Rename(cr_path);
  // This creates the .crdownload version of the file.
  download_file->Initialize(false);
  // |download_file| is owned by DownloadFileManager.
  AddDownloadToFileManager(info->download_id, download_file);

  info->path = new_path;
  AttachDownloadItem(info);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  // Finish the download.
  OnAllDataSaved(0, kTestDataLen, "");
  message_loop_.RunAllPending();

  // Download is complete.
  EXPECT_TRUE(GetActiveDownloadItem(0) == NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_TRUE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_EQ(DownloadItem::COMPLETE, download->state());

  EXPECT_TRUE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));
  EXPECT_FALSE(file_util::PathExists(unique_new_path));
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(new_path, &file_contents));
  EXPECT_EQ(std::string(kTestData), file_contents);
}
