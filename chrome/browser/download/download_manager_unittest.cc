// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/download/download_buffer.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/download/mock_download_file.h"
#include "content/browser/download/mock_download_manager.h"
#include "content/test/test_browser_thread.h"
#include "grit/generated_resources.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if defined(USE_AURA) && defined(OS_WIN)
// http://crbug.com/105200
#define MAYBE_StartDownload DISABLED_StartDownload
#define MAYBE_DownloadOverwriteTest DISABLED_DownloadOverwriteTest
#define MAYBE_DownloadRemoveTest DISABLED_DownloadRemoveTest
#else
#define MAYBE_StartDownload StartDownload
#define MAYBE_DownloadOverwriteTest DownloadOverwriteTest
#define MAYBE_DownloadRemoveTest DownloadRemoveTest
#endif

using content::BrowserThread;

DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class DownloadManagerTest : public testing::Test {
 public:
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : profile_(new TestingProfile()),
        download_manager_delegate_(new ChromeDownloadManagerDelegate(
            profile_.get())),
        id_factory_(new DownloadIdFactory(kValidIdDomain)),
        download_manager_(new DownloadManagerImpl(
            download_manager_delegate_,
            id_factory_,
            &download_status_updater_)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        download_buffer_(new content::DownloadBuffer) {
    download_manager_->Init(profile_.get());
    download_manager_delegate_->SetDownloadManager(download_manager_);
  }

  ~DownloadManagerTest() {
    download_manager_->Shutdown();
    // profile_ must outlive download_manager_, so we explicitly delete
    // download_manager_ first.
    download_manager_ = NULL;
    download_manager_delegate_ = NULL;
    profile_.reset(NULL);
    message_loop_.RunAllPending();
  }

  void AddDownloadToFileManager(int id, DownloadFile* download_file) {
    file_manager()->downloads_[DownloadId(kValidIdDomain, id)] =
      download_file;
  }

  void OnResponseCompleted(int32 download_id, int64 size,
                           const std::string& hash) {
    download_manager_->OnResponseCompleted(download_id, size, hash);
  }

  void FileSelected(const FilePath& path, void* params) {
    download_manager_->FileSelected(path, params);
  }

  void ContinueDownloadWithPath(DownloadItem* download, const FilePath& path) {
    download_manager_->ContinueDownloadWithPath(download, path);
  }

  void UpdateData(int32 id, const char* data, size_t length) {
    // We are passing ownership of this buffer to the download file manager.
    net::IOBuffer* io_buffer = new net::IOBuffer(length);
    // We need |AddRef()| because we do a |Release()| in |UpdateDownload()|.
    io_buffer->AddRef();
    memcpy(io_buffer->data(), data, length);

    download_buffer_->AddData(io_buffer, length);

    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::UpdateDownload, file_manager_.get(),
                   DownloadId(kValidIdDomain, id), download_buffer_));

    message_loop_.RunAllPending();
  }

  void OnDownloadInterrupted(int32 download_id, int64 size,
                             InterruptReason reason) {
    download_manager_->OnDownloadInterrupted(download_id, size, reason);
  }

  // Get the download item with ID |id|.
  DownloadItem* GetActiveDownloadItem(int32 id) {
    return download_manager_->GetActiveDownload(id);
  }

 protected:
  DownloadStatusUpdater download_status_updater_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ChromeDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadIdFactory> id_factory_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<DownloadFileManager> file_manager_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<content::DownloadBuffer> download_buffer_;

  DownloadFileManager* file_manager() {
    if (!file_manager_) {
      file_manager_ = new DownloadFileManager(NULL);
      download_manager_->SetFileManager(file_manager_);
    }
    return file_manager_;
  }

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTest);
};

const char* DownloadManagerTest::kTestData = "a;sdlfalsdfjalsdkfjad";
const size_t DownloadManagerTest::kTestDataLen =
    strlen(DownloadManagerTest::kTestData);

// A DownloadFile that we can inject errors into.
class DownloadFileWithErrors : public DownloadFileImpl {
 public:
  DownloadFileWithErrors(DownloadCreateInfo* info, DownloadManager* manager);
  virtual ~DownloadFileWithErrors() {}

  // BaseFile delegated functions.
  virtual net::Error Initialize(bool calculate_hash);
  virtual net::Error AppendDataToFile(const char* data, size_t data_len);
  virtual net::Error Rename(const FilePath& full_path);

  void set_forced_error(net::Error error) { forced_error_ = error; }
  void clear_forced_error() { forced_error_ = net::OK; }
  net::Error forced_error() const { return forced_error_; }

 private:
  net::Error ReturnError(net::Error function_error) {
    if (forced_error_ != net::OK) {
      net::Error ret = forced_error_;
      clear_forced_error();
      return ret;
    }

    return function_error;
  }

  net::Error forced_error_;
};

DownloadFileWithErrors::DownloadFileWithErrors(DownloadCreateInfo* info,
                                               DownloadManager* manager)
    : DownloadFileImpl(info, new DownloadRequestHandle(), manager),
      forced_error_(net::OK) {
}

net::Error DownloadFileWithErrors::Initialize(bool calculate_hash) {
  return ReturnError(DownloadFileImpl::Initialize(calculate_hash));
}

net::Error DownloadFileWithErrors::AppendDataToFile(const char* data,
                                                    size_t data_len) {
  return ReturnError(DownloadFileImpl::AppendDataToFile(data, data_len));
}

net::Error DownloadFileWithErrors::Rename(const FilePath& full_path) {
  return ReturnError(DownloadFileImpl::Rename(full_path));
}

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
  DownloadStateInfo::DangerType danger;
  bool finish_before_rename;
  int expected_rename_count;
} kDownloadRenameCases[] = {
  // Safe download, download finishes BEFORE file name determined.
  // Renamed twice (linear path through UI).  Crdownload file does not need
  // to be deleted.
  { FILE_PATH_LITERAL("foo.zip"), DownloadStateInfo::NOT_DANGEROUS, true, 2, },
  // Potentially dangerous download (e.g., file is dangerous), download finishes
  // BEFORE file name determined. Needs to be renamed only once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    DownloadStateInfo::MAYBE_DANGEROUS_CONTENT, true, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    DownloadStateInfo::DANGEROUS_FILE, true, 1, },
  // Safe download, download finishes AFTER file name determined.
  // Needs to be renamed twice.
  { FILE_PATH_LITERAL("foo.zip"), DownloadStateInfo::NOT_DANGEROUS, false, 2, },
  // Potentially dangerous download, download finishes AFTER file name
  // determined. Needs to be renamed only once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    DownloadStateInfo::MAYBE_DANGEROUS_CONTENT, false, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.crdownload"),
    DownloadStateInfo::DANGEROUS_FILE, false, 1, },
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
    states_hit_ |= (1 << download->GetState());
    was_updated_ = true;
  }
  virtual void OnDownloadOpened(DownloadItem* download) {
    DCHECK_EQ(tracked_, download);
    states_hit_ |= (1 << download->GetState());
    was_opened_ = true;
  }

  DownloadItem* tracked_;
  int states_hit_;
  bool was_updated_;
  bool was_opened_;
};

}  // namespace

TEST_F(DownloadManagerTest, MAYBE_StartDownload) {
  content::TestBrowserThread io_thread(BrowserThread::IO, &message_loop_);
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetFilePath(prefs::kDownloadDefaultDirectory, FilePath());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromDownloadManager(download_manager_);
  download_prefs->EnableAutoOpenBasedOnExtension(
      FilePath(FILE_PATH_LITERAL("example.pdf")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStartDownloadCases); ++i) {
    prefs->SetBoolean(prefs::kPromptForDownload,
                      kStartDownloadCases[i].prompt_for_download);

    SelectFileObserver observer(download_manager_);
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
    info->download_id = DownloadId(kValidIdDomain, static_cast<int>(i));
    info->prompt_user_for_save_location = kStartDownloadCases[i].save_as;
    info->url_chain.push_back(GURL(kStartDownloadCases[i].url));
    info->mime_type = kStartDownloadCases[i].mime_type;
    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

    DownloadFile* download_file(
        new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                             download_manager_));
    AddDownloadToFileManager(info->download_id.local(), download_file);
    download_file->Initialize(false);
    download_manager_->StartDownload(info->download_id.local());
    message_loop_.RunAllPending();

    // SelectFileObserver will have recorded any attempt to open the
    // select file dialog.
    // Note that DownloadManager::FileSelectionCanceled() is never called.
    EXPECT_EQ(kStartDownloadCases[i].expected_save_as,
              observer.ShowedFileDialogForId(i));
  }
}

TEST_F(DownloadManagerTest, DownloadRenameTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDownloadRenameCases); ++i) {
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
    info->download_id = DownloadId(kValidIdDomain, static_cast<int>(i));
    info->prompt_user_for_save_location = false;
    info->url_chain.push_back(GURL());
    const FilePath new_path(kDownloadRenameCases[i].suggested_path);

    MockDownloadFile::StatisticsRecorder recorder;
    MockDownloadFile* download_file(
        new MockDownloadFile(info.get(),
                             DownloadRequestHandle(),
                             download_manager_,
                             &recorder));
    AddDownloadToFileManager(info->download_id.local(), download_file);

    // |download_file| is owned by DownloadFileManager.
    if (kDownloadRenameCases[i].expected_rename_count == 1) {
      download_file->SetExpectedPath(0, new_path);
    } else {
      ASSERT_EQ(2, kDownloadRenameCases[i].expected_rename_count);
      FilePath crdownload(download_util::GetCrDownloadPath(new_path));
      download_file->SetExpectedPath(0, crdownload);
      download_file->SetExpectedPath(1, new_path);
    }
    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
    DownloadItem* download = GetActiveDownloadItem(i);
    ASSERT_TRUE(download != NULL);
    DownloadStateInfo state = download->GetStateInfo();
    state.danger = kDownloadRenameCases[i].danger;
    download->SetFileCheckResults(state);

    int32* id_ptr = new int32;
    *id_ptr = i;  // Deleted in FileSelected().
    if (kDownloadRenameCases[i].finish_before_rename) {
      OnResponseCompleted(i, 1024, std::string("fake_hash"));
      message_loop_.RunAllPending();
      FileSelected(new_path, id_ptr);
    } else {
      FileSelected(new_path, id_ptr);
      message_loop_.RunAllPending();
      OnResponseCompleted(i, 1024, std::string("fake_hash"));
    }
    message_loop_.RunAllPending();
    EXPECT_EQ(
        kDownloadRenameCases[i].expected_rename_count,
        recorder.Count(MockDownloadFile::StatisticsRecorder::STAT_RENAME));
  }
}

TEST_F(DownloadManagerTest, DownloadInterruptTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->download_id = DownloadId(kValidIdDomain, 0);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->total_bytes = static_cast<int64>(kTestDataLen);
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(download_util::GetCrDownloadPath(new_path));

  MockDownloadFile::StatisticsRecorder recorder;
  MockDownloadFile* download_file(
      new MockDownloadFile(info.get(),
                           DownloadRequestHandle(),
                           download_manager_,
                           &recorder));
  AddDownloadToFileManager(info->download_id.local(), download_file);

  // |download_file| is owned by DownloadFileManager.
  download_file->SetExpectedPath(0, cr_path);

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  scoped_ptr<DownloadItemModel> download_item_model(
      new DownloadItemModel(download));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_EQ(1,
            recorder.Count(MockDownloadFile::StatisticsRecorder::STAT_RENAME));
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  int64 error_size = 3;
  OnDownloadInterrupted(0, error_size,
                        DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(0) == NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  ui::DataUnits amount_units = ui::GetByteDisplayUnits(kTestDataLen);
  string16 simple_size =
      ui::FormatBytesWithUnits(error_size, amount_units, false);
  string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
      ui::FormatBytesWithUnits(kTestDataLen, amount_units, true));
  EXPECT_EQ(download_item_model->GetStatusText(),
            l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED,
                                       simple_size,
                                       simple_total));

  download->Cancel(true);

  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(download->GetReceivedBytes(), error_size);
  EXPECT_EQ(download->GetTotalBytes(), static_cast<int64>(kTestDataLen));
}

// Test the behavior of DownloadFileManager and DownloadManager in the event
// of a file error while writing the download to disk.
TEST_F(DownloadManagerTest, DownloadFileErrorTest) {
  // Create a temporary file and a mock stream.
  FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&path));

  // This file stream will be used, until the first rename occurs.
  net::FileStream* stream = new net::FileStream;
  ASSERT_EQ(0, stream->Open(
      path,
      base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE));

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  static const int32 local_id = 0;
  info->download_id = DownloadId(kValidIdDomain, local_id);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->total_bytes = static_cast<int64>(kTestDataLen * 3);
  info->save_info.file_path = path;
  info->save_info.file_stream.reset(stream);

  // Create a download file that we can insert errors into.
  DownloadFileWithErrors* download_file(new DownloadFileWithErrors(
      info.get(), download_manager_));
  AddDownloadToFileManager(local_id, download_file);

  // |download_file| is owned by DownloadFileManager.
  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  // This will keep track of what should be displayed on the shelf.
  scoped_ptr<DownloadItemModel> download_item_model(
      new DownloadItemModel(download));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Add some data before finalizing the file name.
  UpdateData(local_id, kTestData, kTestDataLen);

  // Finalize the file name.
  ContinueDownloadWithPath(download, path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  // Add more data.
  UpdateData(local_id, kTestData, kTestDataLen);

  // Add more data, but an error occurs.
  download_file->set_forced_error(net::ERR_FAILED);
  UpdateData(local_id, kTestData, kTestDataLen);

  // Check the state.  The download should have been interrupted.
  EXPECT_TRUE(GetActiveDownloadItem(0) == NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());

  // Check the download shelf's information.
  size_t error_size = kTestDataLen * 3;
  size_t total_size = kTestDataLen * 3;
  ui::DataUnits amount_units = ui::GetByteDisplayUnits(kTestDataLen);
  string16 simple_size =
      ui::FormatBytesWithUnits(error_size, amount_units, false);
  string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
      ui::FormatBytesWithUnits(total_size, amount_units, true));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED,
                                       simple_size,
                                       simple_total),
            download_item_model->GetStatusText());

  // Clean up.
  download->Cancel(true);
  message_loop_.RunAllPending();
}

TEST_F(DownloadManagerTest, DownloadCancelTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->download_id = DownloadId(kValidIdDomain, 0);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(download_util::GetCrDownloadPath(new_path));

  MockDownloadFile* download_file(
      new MockDownloadFile(info.get(),
                           DownloadRequestHandle(),
                           download_manager_,
                           NULL));
  AddDownloadToFileManager(info->download_id.local(), download_file);

  // |download_file| is owned by DownloadFileManager.
  download_file->SetExpectedPath(0, cr_path);

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  scoped_ptr<DownloadItemModel> download_item_model(
      new DownloadItemModel(download));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  ContinueDownloadWithPath(download, new_path);
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
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::CANCELLED, download->GetState());
  EXPECT_EQ(download_item_model->GetStatusText(),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELED));

  EXPECT_FALSE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));
}

TEST_F(DownloadManagerTest, MAYBE_DownloadOverwriteTest) {
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
  int uniquifier = DownloadFile::GetUniquePathNumber(new_path);
  FilePath unique_new_path = new_path;
  EXPECT_NE(0, uniquifier);
  DownloadFile::AppendNumberToPath(&unique_new_path, uniquifier);

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->download_id = DownloadId(kValidIdDomain, 0);
  info->prompt_user_for_save_location = true;
  info->url_chain.push_back(GURL());

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  scoped_ptr<DownloadItemModel> download_item_model(
      new DownloadItemModel(download));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                           download_manager_));
  download_file->Rename(cr_path);
  // This creates the .crdownload version of the file.
  download_file->Initialize(false);
  // |download_file| is owned by DownloadFileManager.
  AddDownloadToFileManager(info->download_id.local(), download_file);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  // Finish the download.
  OnResponseCompleted(0, kTestDataLen, "");
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
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::COMPLETE, download->GetState());
  EXPECT_EQ(download_item_model->GetStatusText(), string16());

  EXPECT_TRUE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));
  EXPECT_FALSE(file_util::PathExists(unique_new_path));
  std::string file_contents;
  EXPECT_TRUE(file_util::ReadFileToString(new_path, &file_contents));
  EXPECT_EQ(std::string(kTestData), file_contents);
}

TEST_F(DownloadManagerTest, MAYBE_DownloadRemoveTest) {
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

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->download_id = DownloadId(kValidIdDomain, 0);
  info->prompt_user_for_save_location = true;
  info->url_chain.push_back(GURL());

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  scoped_ptr<DownloadItemModel> download_item_model(
      new DownloadItemModel(download));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                           download_manager_));
  download_file->Rename(cr_path);
  // This creates the .crdownload version of the file.
  download_file->Initialize(false);
  // |download_file| is owned by DownloadFileManager.
  AddDownloadToFileManager(info->download_id.local(), download_file);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  // Finish the download.
  OnResponseCompleted(0, kTestDataLen, "");
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
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::COMPLETE, download->GetState());
  EXPECT_EQ(download_item_model->GetStatusText(), string16());

  EXPECT_TRUE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));

  // Remove the downloaded file.
  ASSERT_TRUE(file_util::Delete(new_path, false));
  download->OnDownloadedFileRemoved();
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(0) == NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_FALSE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_TRUE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_TRUE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::COMPLETE, download->GetState());
  EXPECT_EQ(download_item_model->GetStatusText(),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_REMOVED));

  EXPECT_FALSE(file_util::PathExists(new_path));
}
