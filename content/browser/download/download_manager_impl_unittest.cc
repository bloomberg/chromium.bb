// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/download_buffer.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/mock_download_file.h"
#include "content/browser/power_save_blocker.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/test/mock_download_manager.h"
#include "content/test/test_browser_context.h"
#include "content/test/test_browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA) && defined(OS_WIN)
// http://crbug.com/105200
#define MAYBE_StartDownload DISABLED_StartDownload
#define MAYBE_DownloadOverwriteTest DISABLED_DownloadOverwriteTest
#define MAYBE_DownloadRemoveTest DISABLED_DownloadRemoveTest
#define MAYBE_DownloadFileErrorTest DownloadFileErrorTest
#elif defined(OS_LINUX)
// http://crbug.com/110886 for Linux
#define MAYBE_StartDownload DISABLED_StartDownload
#define MAYBE_DownloadOverwriteTest DISABLED_DownloadOverwriteTest
#define MAYBE_DownloadRemoveTest DISABLED_DownloadRemoveTest
#define MAYBE_DownloadFileErrorTest DISABLED_DownloadFileErrorTest
#else
#define MAYBE_StartDownload StartDownload
#define MAYBE_DownloadOverwriteTest DownloadOverwriteTest
#define MAYBE_DownloadRemoveTest DownloadRemoveTest
#define MAYBE_DownloadFileErrorTest DownloadFileErrorTest
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadFile;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::Return;

namespace {

FilePath GetTempDownloadPath(const FilePath& suggested_path) {
  return FilePath(suggested_path.value() + FILE_PATH_LITERAL(".temp"));
}

class MockDownloadFileFactory
    : public DownloadFileManager::DownloadFileFactory {
 public:
  MockDownloadFileFactory() {}

  virtual DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle,
      DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
};

DownloadFile* MockDownloadFileFactory::CreateFile(
    DownloadCreateInfo* info,
    const DownloadRequestHandle& request_handle,
    DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {
  NOTREACHED();
  return NULL;
}

DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class TestDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  TestDownloadManagerDelegate()
      : mark_content_dangerous_(false),
        prompt_user_for_save_location_(false),
        download_manager_(NULL) {
  }

  void set_download_manager(content::DownloadManager* dm) {
    download_manager_ = dm;
  }

  void set_prompt_user_for_save_location(bool value) {
    prompt_user_for_save_location_ = value;
  }

  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE {
    DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
    DownloadStateInfo state = item->GetStateInfo();
    FilePath path = net::GenerateFileName(item->GetURL(),
                                          item->GetContentDisposition(),
                                          item->GetReferrerCharset(),
                                          item->GetSuggestedFilename(),
                                          item->GetMimeType(),
                                          std::string());
    if (!ShouldOpenFileBasedOnExtension(path) && prompt_user_for_save_location_)
      state.prompt_user_for_save_location = true;
    item->SetFileCheckResults(state);
    return true;
  }

  virtual FilePath GetIntermediatePath(
      const FilePath& suggested_path) OVERRIDE {
    return GetTempDownloadPath(suggested_path);
  }

  virtual void ChooseDownloadPath(WebContents* web_contents,
                                  const FilePath& suggested_path,
                                  int32 download_id) OVERRIDE {
    if (!expected_suggested_path_.empty()) {
      EXPECT_STREQ(expected_suggested_path_.value().c_str(),
                   suggested_path.value().c_str());
    }
    if (file_selection_response_.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::FileSelectionCanceled,
                     download_manager_,
                     download_id));
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::FileSelected,
                     download_manager_,
                     file_selection_response_,
                     download_id));
    }
    expected_suggested_path_.clear();
    file_selection_response_.clear();
  }

  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE {
    return path.Extension() == FilePath::StringType(FILE_PATH_LITERAL(".pdf"));
  }

  virtual void AddItemToPersistentStore(DownloadItem* item) OVERRIDE {
    static int64 db_handle = DownloadItem::kUninitializedHandle;
    download_manager_->OnItemAddedToPersistentStore(item->GetId(), --db_handle);
  }

  void SetFileSelectionExpectation(const FilePath& suggested_path,
                                   const FilePath& response) {
    expected_suggested_path_ = suggested_path;
    file_selection_response_ = response;
  }

  void SetMarkContentsDangerous(bool dangerous) {
    mark_content_dangerous_ = dangerous;
  }

  virtual bool ShouldCompleteDownload(
      DownloadItem* item,
      const base::Closure& complete_callback) {
    if (mark_content_dangerous_) {
      CHECK(!complete_callback.is_null());
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&TestDownloadManagerDelegate::MarkContentDangerous,
                     base::Unretained(this), item->GetId(), complete_callback));
      mark_content_dangerous_ = false;
      return false;
    } else {
      return true;
    }
  }

 private:
  void MarkContentDangerous(
      int32 download_id,
      const base::Closure& complete_callback) {
    DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
    if (!item)
      return;
    item->SetDangerType(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
    complete_callback.Run();
  }

  FilePath expected_suggested_path_;
  FilePath file_selection_response_;
  bool mark_content_dangerous_;
  bool prompt_user_for_save_location_;
  DownloadManager* download_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadManagerDelegate);
};

} // namespace

class DownloadManagerTest : public testing::Test {
 public:
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : browser_context(new TestBrowserContext()),
        download_manager_delegate_(new TestDownloadManagerDelegate()),
        download_manager_(new DownloadManagerImpl(
            download_manager_delegate_.get(), NULL)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        download_buffer_(new content::DownloadBuffer) {
    download_manager_->Init(browser_context.get());
    download_manager_delegate_->set_download_manager(download_manager_);
  }

  ~DownloadManagerTest() {
    download_manager_->Shutdown();
    // browser_context must outlive download_manager_, so we explicitly delete
    // download_manager_ first.
    download_manager_ = NULL;
    download_manager_delegate_.reset();
    browser_context.reset(NULL);
    message_loop_.RunAllPending();
  }

  void AddDownloadToFileManager(int id, DownloadFile* download_file) {
    file_manager()->downloads_[DownloadId(kValidIdDomain, id)] =
      download_file;
  }

  void AddMockDownloadToFileManager(int id, MockDownloadFile* download_file) {
    AddDownloadToFileManager(id, download_file);
    ON_CALL(*download_file, GetDownloadManager())
        .WillByDefault(Return(download_manager_));
  }

  void OnResponseCompleted(int32 download_id, int64 size,
                           const std::string& hash) {
    download_manager_->OnResponseCompleted(download_id, size, hash);
  }

  void FileSelected(const FilePath& path, int32 download_id) {
    download_manager_->FileSelected(path, download_id);
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
                             const std::string& hash_state,
                             content::DownloadInterruptReason reason) {
    download_manager_->OnDownloadInterrupted(download_id, size,
                                             hash_state, reason);
  }

  // Get the download item with ID |id|.
  DownloadItem* GetActiveDownloadItem(int32 id) {
    return download_manager_->GetActiveDownload(id);
  }

 protected:
  scoped_ptr<TestBrowserContext> browser_context;
  scoped_ptr<TestDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManagerImpl> download_manager_;
  scoped_refptr<DownloadFileManager> file_manager_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<content::DownloadBuffer> download_buffer_;

  DownloadFileManager* file_manager() {
    if (!file_manager_) {
      file_manager_ = new DownloadFileManager(new MockDownloadFileFactory);
      download_manager_->SetFileManagerForTesting(file_manager_);
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
  DownloadFileWithErrors(DownloadCreateInfo* info,
                         DownloadManager* manager,
                         bool calculate_hash);
  virtual ~DownloadFileWithErrors() {}

  // BaseFile delegated functions.
  virtual net::Error Initialize();
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
                                               DownloadManager* manager,
                                               bool calculate_hash)
    : DownloadFileImpl(info,
                       new DownloadRequestHandle(),
                       manager,
                       calculate_hash,
                       scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                       net::BoundNetLog()),
      forced_error_(net::OK) {
}

net::Error DownloadFileWithErrors::Initialize() {
  return ReturnError(DownloadFileImpl::Initialize());
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
  content::DownloadDangerType danger;
  bool finish_before_rename;
  int expected_rename_count;
} kDownloadRenameCases[] = {
  // Safe download, download finishes BEFORE file name determined.
  // Renamed twice (linear path through UI).  temp file does not need
  // to be deleted.
  { FILE_PATH_LITERAL("foo.zip"), content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    true, 2, },
  // Potentially dangerous download (e.g., file is dangerous), download finishes
  // BEFORE file name determined. Needs to be renamed only once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.temp"),
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, true, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.temp"),
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE, true, 1, },
  // Safe download, download finishes AFTER file name determined.
  // Needs to be renamed twice.
  { FILE_PATH_LITERAL("foo.zip"), content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    false, 2, },
  // Potentially dangerous download, download finishes AFTER file name
  // determined. Needs to be renamed only once.
  { FILE_PATH_LITERAL("Unconfirmed xxx.temp"),
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, false, 1, },
  { FILE_PATH_LITERAL("Unconfirmed xxx.temp"),
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE, false, 1, },
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
  virtual void ModelChanged(DownloadManager* manager) OVERRIDE {}
  virtual void ManagerGoingDown(DownloadManager* manager) OVERRIDE {}
  virtual void SelectFileDialogDisplayed(
      DownloadManager* manager, int32 id) OVERRIDE {
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
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStartDownloadCases); ++i) {
    download_manager_delegate_->set_prompt_user_for_save_location(
        kStartDownloadCases[i].prompt_for_download);

    SelectFileObserver observer(download_manager_);
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
    const DownloadId id = DownloadId(kValidIdDomain, static_cast<int>(i));
    info->download_id = id;
    info->prompt_user_for_save_location = kStartDownloadCases[i].save_as;
    info->url_chain.push_back(GURL(kStartDownloadCases[i].url));
    info->mime_type = kStartDownloadCases[i].mime_type;
    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

    DownloadFile* download_file(
        new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                             download_manager_, false,
                             scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                             net::BoundNetLog()));
    AddDownloadToFileManager(info->download_id.local(), download_file);
    download_file->Initialize();
    download_manager_->StartDownload(info->download_id.local());
    message_loop_.RunAllPending();

    // SelectFileObserver will have recorded any attempt to open the
    // select file dialog.
    // Note that DownloadManager::FileSelectionCanceled() is never called.
    EXPECT_EQ(kStartDownloadCases[i].expected_save_as,
              observer.ShowedFileDialogForId(i));

    file_manager()->CancelDownload(id);
  }
}

namespace {

enum PromptForSaveLocation {
  DONT_PROMPT,
  PROMPT
};

enum ValidateDangerousDownload {
  DONT_VALIDATE,
  VALIDATE
};

// Test cases to be used with DownloadFilenameTest.  The paths that are used in
// test cases can contain "$dl" and "$alt" tokens which are replaced by a
// default download path, and an alternate download path in
// ExpandFilenameTestPath() below.
const struct DownloadFilenameTestCase {

  // Fields to be set in DownloadStateInfo when calling SetFileCheckResults().
  const FilePath::CharType*     suggested_path;
  const FilePath::CharType*     target_name;
  PromptForSaveLocation         prompt_user_for_save_location;
  content::DownloadDangerType   danger_type;

  // If we receive a ChooseDownloadPath() call to prompt the user for a download
  // location, |prompt_path| is the expected prompt path. The
  // TestDownloadManagerDelegate will respond with |final_path|. If |final_path|
  // is empty, then the file choose dialog be cancelled.
  const FilePath::CharType*     prompt_path;

  // The expected intermediate path for the download.
  const FilePath::CharType*     intermediate_path;

  // The expected final path for the download.
  const FilePath::CharType*     final_path;

  // If this is a dangerous download, then we will either validate the download
  // or delete it depending on the value of |validate_dangerous_download|.
  ValidateDangerousDownload     validate_dangerous_download;
} kDownloadFilenameTestCases[] = {
  {
    // 0: A safe file is downloaded with no prompting.
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL(""),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/foo.txt.temp"),
    FILE_PATH_LITERAL("$dl/foo.txt"),
    DONT_VALIDATE
  },
  {
    // 1: A safe file is downloaded with prompting.
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL(""),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL("$dl/foo.txt.temp"),
    FILE_PATH_LITERAL("$dl/foo.txt"),
    DONT_VALIDATE
  },
  {
    // 2: A safe file is downloaded. The filename is changed before the dialog
    // completes.
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL(""),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL("$dl/bar.txt.temp"),
    FILE_PATH_LITERAL("$dl/bar.txt"),
    DONT_VALIDATE
  },
  {
    // 3: A safe file is downloaded. The download path is changed before the
    // dialog completes.
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL(""),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    FILE_PATH_LITERAL("$dl/foo.txt"),
    FILE_PATH_LITERAL("$alt/bar.txt.temp"),
    FILE_PATH_LITERAL("$alt/bar.txt"),
    DONT_VALIDATE
  },
  {
    // 4: Potentially dangerous content.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/foo.exe"),
    DONT_VALIDATE
  },
  {
    // 5: Potentially dangerous content. Uses "Save as."
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL("$dl/foo.exe"),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/foo.exe"),
    DONT_VALIDATE
  },
  {
    // 6: Potentially dangerous content. Uses "Save as." The download filename
    // is changed before the dialog completes.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL("$dl/foo.exe"),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/bar.exe"),
    DONT_VALIDATE
  },
  {
    // 7: Potentially dangerous content. Uses "Save as." The download directory
    // is changed before the dialog completes.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL("$dl/foo.exe"),
    FILE_PATH_LITERAL("$alt/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$alt/bar.exe"),
    DONT_VALIDATE
  },
  {
    // 8: Dangerous content. Saved directly.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/foo.exe"),
    VALIDATE
  },
  {
    // 9: Dangerous content. Saved directly. Not validated.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL(""),
    DONT_VALIDATE
  },
  {
    // 10: Dangerous content. Uses "Save as." The download directory is changed
    // before the dialog completes.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("foo.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
    FILE_PATH_LITERAL("$dl/foo.exe"),
    FILE_PATH_LITERAL("$alt/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$alt/bar.exe"),
    VALIDATE
  },
  {
    // 11: A safe file is download. The target file exists, but we don't
    // uniquify. Safe downloads are uniquified in ChromeDownloadManagerDelegate
    // instead of DownloadManagerImpl.
    FILE_PATH_LITERAL("$dl/exists.txt"),
    FILE_PATH_LITERAL(""),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/exists.txt.temp"),
    FILE_PATH_LITERAL("$dl/exists.txt"),
    DONT_VALIDATE
  },
  {
    // 12: A potentially dangerous file is download. The target file exists. The
    // target path is uniquified.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("exists.exe"),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/exists (1).exe"),
    DONT_VALIDATE
  },
  {
    // 13: A dangerous file is download. The target file exists. The target path
    // is uniquified.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("exists.exe"),
    DONT_PROMPT,
    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/exists (1).exe"),
    VALIDATE
  },
  {
    // 14: A potentially dangerous file is download with prompting. The target
    // file exists. The target path is not uniquified because the filename was
    // given to us by the user.
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("exists.exe"),
    PROMPT,
    content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
    FILE_PATH_LITERAL("$dl/exists.exe"),
    FILE_PATH_LITERAL("$dl/Unconfirmed xxx.download"),
    FILE_PATH_LITERAL("$dl/exists.exe"),
    DONT_VALIDATE
  },
};

FilePath ExpandFilenameTestPath(const FilePath::CharType* template_path,
                                const FilePath& downloads_dir,
                                const FilePath& alternate_dir) {
  FilePath::StringType path(template_path);
  ReplaceSubstringsAfterOffset(&path, 0, FILE_PATH_LITERAL("$dl"),
                               downloads_dir.value());
  ReplaceSubstringsAfterOffset(&path, 0, FILE_PATH_LITERAL("$alt"),
                               alternate_dir.value());
  return FilePath(path).NormalizePathSeparators();
}

} // namespace

TEST_F(DownloadManagerTest, DownloadFilenameTest) {
  ScopedTempDir scoped_dl_dir;
  ASSERT_TRUE(scoped_dl_dir.CreateUniqueTempDir());

  FilePath downloads_dir(scoped_dl_dir.path());
  FilePath alternate_dir(downloads_dir.Append(FILE_PATH_LITERAL("Foo")));

  // We create a known file to test file uniquification.
  file_util::WriteFile(downloads_dir.Append(FILE_PATH_LITERAL("exists.txt")),
                       "", 0);
  file_util::WriteFile(downloads_dir.Append(FILE_PATH_LITERAL("exists.exe")),
                       "", 0);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDownloadFilenameTestCases); ++i) {
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
    info->download_id = DownloadId(kValidIdDomain, i);
    info->url_chain.push_back(GURL());

    MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());
    FilePath suggested_path(ExpandFilenameTestPath(
        kDownloadFilenameTestCases[i].suggested_path,
        downloads_dir, alternate_dir));
    FilePath prompt_path(ExpandFilenameTestPath(
        kDownloadFilenameTestCases[i].prompt_path,
        downloads_dir, alternate_dir));
    FilePath intermediate_path(ExpandFilenameTestPath(
        kDownloadFilenameTestCases[i].intermediate_path,
        downloads_dir, alternate_dir));
    FilePath final_path(ExpandFilenameTestPath(
        kDownloadFilenameTestCases[i].final_path,
        downloads_dir, alternate_dir));

    AddMockDownloadToFileManager(info->download_id.local(), download_file);

    EXPECT_CALL(*download_file, Rename(intermediate_path))
        .WillOnce(Return(net::OK));

    if (!final_path.empty()) {
      // If |final_path| is empty, its a signal that the download doesn't
      // complete.  Therefore it will only go through a single rename.
      EXPECT_CALL(*download_file, Rename(final_path))
          .WillOnce(Return(net::OK));
    }

    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
    DownloadItem* download = GetActiveDownloadItem(i);
    ASSERT_TRUE(download != NULL);

    DownloadStateInfo state = download->GetStateInfo();
    state.suggested_path = suggested_path;
    state.danger = kDownloadFilenameTestCases[i].danger_type;
    state.prompt_user_for_save_location =
        (kDownloadFilenameTestCases[i].prompt_user_for_save_location == PROMPT);
    state.target_name = FilePath(kDownloadFilenameTestCases[i].target_name);
    if (state.danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT) {
      // DANGEROUS_CONTENT will only be known once we have all the data. We let
      // our TestDownloadManagerDelegate handle it.
      state.danger = content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
      download_manager_delegate_->SetMarkContentsDangerous(true);
    }
    download->SetFileCheckResults(state);
    download_manager_delegate_->SetFileSelectionExpectation(
        prompt_path, final_path);
    download_manager_->RestartDownload(i);
    message_loop_.RunAllPending();

    OnResponseCompleted(i, 1024, std::string("fake_hash"));
    message_loop_.RunAllPending();

    if (download->GetSafetyState() == DownloadItem::DANGEROUS) {
      if (kDownloadFilenameTestCases[i].validate_dangerous_download == VALIDATE)
        download->DangerousDownloadValidated();
      else
        download->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
      message_loop_.RunAllPending();
      // |download| might be deleted when we get here.
    }
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

    MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());
    const DownloadId id = info->download_id;
    ON_CALL(*download_file, GlobalId())
        .WillByDefault(ReturnRef(id));

    AddMockDownloadToFileManager(info->download_id.local(), download_file);

    // |download_file| is owned by DownloadFileManager.
    if (kDownloadRenameCases[i].expected_rename_count == 1) {
      EXPECT_CALL(*download_file, Rename(new_path))
          .Times(1)
          .WillOnce(Return(net::OK));
    } else {
      ASSERT_EQ(2, kDownloadRenameCases[i].expected_rename_count);
      FilePath temp(GetTempDownloadPath(new_path));
      EXPECT_CALL(*download_file, Rename(temp))
          .Times(1)
          .WillOnce(Return(net::OK));
      EXPECT_CALL(*download_file, Rename(new_path))
          .Times(1)
          .WillOnce(Return(net::OK));
    }
    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
    DownloadItem* download = GetActiveDownloadItem(i);
    ASSERT_TRUE(download != NULL);
    DownloadStateInfo state = download->GetStateInfo();
    state.danger = kDownloadRenameCases[i].danger;
    download->SetFileCheckResults(state);

    if (kDownloadRenameCases[i].finish_before_rename) {
      OnResponseCompleted(i, 1024, std::string("fake_hash"));
      message_loop_.RunAllPending();
      FileSelected(new_path, i);
    } else {
      FileSelected(new_path, i);
      message_loop_.RunAllPending();
      OnResponseCompleted(i, 1024, std::string("fake_hash"));
    }
    // Validating the download item, so it will complete.
    if (state.danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE)
      download->DangerousDownloadValidated();
    message_loop_.RunAllPending();
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
  const FilePath cr_path(GetTempDownloadPath(new_path));

  MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());
  ON_CALL(*download_file, AppendDataToFile(_, _))
      .WillByDefault(Return(net::OK));

  // |download_file| is owned by DownloadFileManager.
  AddMockDownloadToFileManager(info->download_id.local(), download_file);

  EXPECT_CALL(*download_file, Rename(cr_path))
      .Times(1)
      .WillOnce(Return(net::OK));

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  download_file->AppendDataToFile(kTestData, kTestDataLen);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(0) != NULL);

  int64 error_size = 3;
  OnDownloadInterrupted(0, error_size, "",
                        content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
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
TEST_F(DownloadManagerTest, MAYBE_DownloadFileErrorTest) {
  // Create a temporary file and a mock stream.
  FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&path));

  // This file stream will be used, until the first rename occurs.
  net::FileStream* stream = new net::FileStream(NULL);
  ASSERT_EQ(0, stream->OpenSync(
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
      info.get(), download_manager_, false));
  download_file->Initialize();
  AddDownloadToFileManager(local_id, download_file);

  // |download_file| is owned by DownloadFileManager.
  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);

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
  DownloadId id = DownloadId(kValidIdDomain, 0);
  info->download_id = id;
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(GetTempDownloadPath(new_path));

  MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());
  ON_CALL(*download_file, AppendDataToFile(_, _))
      .WillByDefault(Return(net::OK));
  AddMockDownloadToFileManager(info->download_id.local(), download_file);

  // |download_file| is owned by DownloadFileManager.
  EXPECT_CALL(*download_file, Rename(cr_path))
      .Times(1)
      .WillOnce(Return(net::OK));

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(0);
  ASSERT_TRUE(download != NULL);

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

  file_manager()->CancelDownload(id);

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
  const FilePath cr_path(GetTempDownloadPath(new_path));
  EXPECT_FALSE(file_util::PathExists(new_path));

  // Create the file that we will overwrite.  Will be automatically cleaned
  // up when temp_dir_ is destroyed.
  FILE* fp = file_util::OpenFile(new_path, "w");
  file_util::CloseFile(fp);
  EXPECT_TRUE(file_util::PathExists(new_path));

  // Construct the unique file name that normally would be created, but
  // which we will override.
  int uniquifier =
      file_util::GetUniquePathNumber(new_path, FILE_PATH_LITERAL(""));
  FilePath unique_new_path = new_path;
  EXPECT_NE(0, uniquifier);
  unique_new_path = unique_new_path.InsertBeforeExtensionASCII(
            StringPrintf(" (%d)", uniquifier));

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

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                           download_manager_, false,
                           scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                           net::BoundNetLog()));
  download_file->Rename(cr_path);
  // This creates the .temp version of the file.
  download_file->Initialize();
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
  const FilePath cr_path(GetTempDownloadPath(new_path));
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

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), new DownloadRequestHandle(),
                           download_manager_, false,
                           scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                           net::BoundNetLog()));
  download_file->Rename(cr_path);
  // This creates the .temp version of the file.
  download_file->Initialize();
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

  EXPECT_FALSE(file_util::PathExists(new_path));
}
