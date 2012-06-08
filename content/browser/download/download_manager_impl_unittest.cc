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
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
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

namespace content {
class ByteStreamReader;
}

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
      scoped_ptr<content::ByteStreamReader> stream,
      const DownloadRequestHandle& request_handle,
      DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
};

DownloadFile* MockDownloadFileFactory::CreateFile(
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    const DownloadRequestHandle& request_handle,
    DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {
  NOTREACHED();
  return NULL;
}

class TestDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  explicit TestDownloadManagerDelegate(content::DownloadManager* dm)
      : mark_content_dangerous_(false),
        prompt_user_for_save_location_(false),
        should_complete_download_(true),
        download_manager_(dm) {
  }

  void set_download_directory(const FilePath& path) {
    download_directory_ = path;
  }

  void set_prompt_user_for_save_location(bool value) {
    prompt_user_for_save_location_ = value;
  }

  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE {
    DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
    FilePath path = net::GenerateFileName(item->GetURL(),
                                          item->GetContentDisposition(),
                                          item->GetReferrerCharset(),
                                          item->GetSuggestedFilename(),
                                          item->GetMimeType(),
                                          std::string());
    DownloadItem::TargetDisposition disposition = item->GetTargetDisposition();
    if (!ShouldOpenFileBasedOnExtension(path) && prompt_user_for_save_location_)
      disposition = DownloadItem::TARGET_DISPOSITION_PROMPT;
    item->OnTargetPathDetermined(download_directory_.Append(path),
                                 disposition,
                                 content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    return true;
  }

  virtual FilePath GetIntermediatePath(const DownloadItem& item,
                                       bool* ok_to_overwrite) OVERRIDE {
    if (intermediate_path_.empty()) {
      *ok_to_overwrite = true;
      return GetTempDownloadPath(item.GetTargetFilePath());
    } else {
      *ok_to_overwrite = overwrite_intermediate_path_;
      return intermediate_path_;
    }
  }

  virtual void ChooseDownloadPath(DownloadItem* item) OVERRIDE {
    if (!expected_suggested_path_.empty()) {
      EXPECT_STREQ(expected_suggested_path_.value().c_str(),
          item->GetTargetFilePath().value().c_str());
    }
    if (file_selection_response_.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::FileSelectionCanceled,
                     download_manager_,
                     item->GetId()));
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DownloadManager::FileSelected,
                     download_manager_,
                     file_selection_response_,
                     item->GetId()));
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

  void SetIntermediatePath(const FilePath& intermediate_path,
                           bool overwrite_intermediate_path) {
    intermediate_path_ = intermediate_path;
    overwrite_intermediate_path_ = overwrite_intermediate_path;
  }

  void SetShouldCompleteDownload(bool value) {
    should_complete_download_ = value;
  }

  void InvokeDownloadCompletionCallback() {
    EXPECT_FALSE(download_completion_callback_.is_null());
    download_completion_callback_.Run();
    download_completion_callback_.Reset();
  }

  virtual bool ShouldCompleteDownload(
      DownloadItem* item,
      const base::Closure& complete_callback) OVERRIDE {
    download_completion_callback_ = complete_callback;
    if (mark_content_dangerous_) {
      CHECK(!complete_callback.is_null());
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&TestDownloadManagerDelegate::MarkContentDangerous,
                     base::Unretained(this), item->GetId()));
      mark_content_dangerous_ = false;
      return false;
    } else {
      return should_complete_download_;
    }
  }

 private:
  void MarkContentDangerous(
      int32 download_id) {
    DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
    if (!item)
      return;
    item->OnContentCheckCompleted(
        content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
    InvokeDownloadCompletionCallback();
  }

  FilePath download_directory_;
  FilePath expected_suggested_path_;
  FilePath file_selection_response_;
  FilePath intermediate_path_;
  bool overwrite_intermediate_path_;
  bool mark_content_dangerous_;
  bool prompt_user_for_save_location_;
  bool should_complete_download_;
  DownloadManager* download_manager_;
  base::Closure download_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadManagerDelegate);
};

} // namespace

class DownloadManagerTest : public testing::Test {
 public:
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : browser_context(new content::TestBrowserContext()),
        download_manager_(new DownloadManagerImpl(NULL)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
    download_manager_delegate_.reset(
        new TestDownloadManagerDelegate(download_manager_.get()));
    download_manager_->SetDelegate(download_manager_delegate_.get());
    download_manager_->Init(browser_context.get());
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

  // Create a temporary directory as the downloads directory.
  bool CreateTempDownloadsDirectory() {
    if (!scoped_download_dir_.CreateUniqueTempDir())
      return false;
    download_manager_delegate_->set_download_directory(
        scoped_download_dir_.path());
    return true;
  }

  void AddDownloadToFileManager(DownloadId id, DownloadFile* download_file) {
    file_manager()->downloads_[id] =
      download_file;
  }

  void AddMockDownloadToFileManager(DownloadId id,
                                    MockDownloadFile* download_file) {
    AddDownloadToFileManager(id, download_file);
    EXPECT_CALL(*download_file, GetDownloadManager())
        .WillRepeatedly(Return(download_manager_));
  }

  void OnResponseCompleted(DownloadId download_id, int64 size,
                           const std::string& hash) {
    download_manager_->OnResponseCompleted(download_id.local(), size, hash);
  }

  void ContinueDownloadWithPath(DownloadItem* download, const FilePath& path) {
    download->OnTargetPathDetermined(
        path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    download_manager_->OnTargetPathAvailable(download);
  }

  void OnDownloadInterrupted(DownloadId download_id, int64 size,
                             const std::string& hash_state,
                             content::DownloadInterruptReason reason) {
    download_manager_->OnDownloadInterrupted(download_id.local(), size,
                                             hash_state, reason);
  }

  // Get the download item with ID |id|.
  DownloadItem* GetActiveDownloadItem(DownloadId id) {
    return download_manager_->GetActiveDownload(id.local());
  }

  FilePath GetPathInDownloadsDir(const FilePath::StringType& fragment) {
    DCHECK(scoped_download_dir_.IsValid());
    FilePath full_path(scoped_download_dir_.path().Append(fragment));
    return full_path.NormalizePathSeparators();
  }

 protected:
  scoped_ptr<content::TestBrowserContext> browser_context;
  scoped_ptr<TestDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManagerImpl> download_manager_;
  scoped_refptr<DownloadFileManager> file_manager_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  ScopedTempDir scoped_download_dir_;

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
                         scoped_ptr<content::ByteStreamReader> stream,
                         DownloadManager* manager,
                         bool calculate_hash);
  virtual ~DownloadFileWithErrors() {}

  // BaseFile delegated functions.
  virtual net::Error Initialize() OVERRIDE;
  virtual net::Error AppendDataToFile(const char* data,
                                      size_t data_len) OVERRIDE;
  virtual net::Error Rename(const FilePath& full_path) OVERRIDE;

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

DownloadFileWithErrors::DownloadFileWithErrors(
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    DownloadManager* manager,
    bool calculate_hash)
    : DownloadFileImpl(info,
                       stream.Pass(),
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
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE {
    DCHECK_EQ(tracked_, download);
    states_hit_ |= (1 << download->GetState());
    was_updated_ = true;
  }
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE{
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
  ASSERT_TRUE(CreateTempDownloadsDirectory());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStartDownloadCases); ++i) {
    DVLOG(1) << "Test case " << i;
    download_manager_delegate_->set_prompt_user_for_save_location(
        kStartDownloadCases[i].prompt_for_download);

    SelectFileObserver observer(download_manager_);
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
    info->prompt_user_for_save_location = kStartDownloadCases[i].save_as;
    info->url_chain.push_back(GURL(kStartDownloadCases[i].url));
    info->mime_type = kStartDownloadCases[i].mime_type;
    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

    scoped_ptr<content::ByteStreamWriter> stream_writer;
    scoped_ptr<content::ByteStreamReader> stream_reader;
    content::CreateByteStream(message_loop_.message_loop_proxy(),
                              message_loop_.message_loop_proxy(),
                              kTestDataLen, &stream_writer, &stream_reader);

    DownloadFile* download_file(
        new DownloadFileImpl(info.get(),
                             stream_reader.Pass(),
                             new DownloadRequestHandle(),
                             download_manager_, false,
                             scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                             net::BoundNetLog()));
    AddDownloadToFileManager(info->download_id, download_file);
    download_file->Initialize();
    download_manager_->StartDownload(info->download_id.local());
    message_loop_.RunAllPending();

    // SelectFileObserver will have recorded any attempt to open the
    // select file dialog.
    // Note that DownloadManager::FileSelectionCanceled() is never called.
    EXPECT_EQ(kStartDownloadCases[i].expected_save_as,
              observer.ShowedFileDialogForId(info->download_id.local()));

    file_manager()->CancelDownload(info->download_id);
  }
}

namespace {

enum OverwriteDownloadPath {
  DONT_OVERWRITE,
  OVERWRITE
};

enum ResponseCompletionTime {
  COMPLETES_BEFORE_RENAME,
  COMPLETES_AFTER_RENAME
};

// Test cases to be used with DownloadFilenameTest.  The paths are relative to
// the temporary downloads directory used for testing.
const struct DownloadFilenameTestCase {
  // DownloadItem properties prior to calling RestartDownload().
  const FilePath::CharType*       target_path;
  DownloadItem::TargetDisposition target_disposition;

  // If we receive a ChooseDownloadPath() call to prompt the user for a download
  // location, |expected_prompt_path| is the expected prompt path. The
  // TestDownloadManagerDelegate will respond with |chosen_path|. If
  // |chosen_path| is empty, then the file choose dialog be cancelled.
  const FilePath::CharType*       expected_prompt_path;
  const FilePath::CharType*       chosen_path;

  // Response to GetIntermediatePath(). If |intermediate_path| is empty, then a
  // temporary path is constructed with
  // GetTempDownloadPath(item->GetTargetFilePath()).
  const FilePath::CharType*       intermediate_path;
  OverwriteDownloadPath           overwrite_intermediate_path;

  // Determines when OnResponseCompleted() is called. If this is
  // COMPLETES_BEFORE_RENAME, then the response completes before the filename is
  // determines.
  ResponseCompletionTime          completion;

  // The expected intermediate path for the download.
  const FilePath::CharType*       expected_intermediate_path;

  // The expected final path for the download.
  const FilePath::CharType*       expected_final_path;
} kDownloadFilenameTestCases[] = {

#define TARGET                FILE_PATH_LITERAL
#define INTERMEDIATE          FILE_PATH_LITERAL
#define EXPECTED_PROMPT       FILE_PATH_LITERAL
#define PROMPT_RESPONSE       FILE_PATH_LITERAL
#define EXPECTED_INTERMEDIATE FILE_PATH_LITERAL
#define EXPECTED_FINAL        FILE_PATH_LITERAL

  {
    // 0: No prompting.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("foo.txt.intermediate"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("foo.txt.intermediate"),
    EXPECTED_FINAL("foo.txt"),
  },
  {
    // 1: With prompting. No rename.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT("foo.txt"),
    PROMPT_RESPONSE("foo.txt"),

    INTERMEDIATE("foo.txt.intermediate"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("foo.txt.intermediate"),
    EXPECTED_FINAL("foo.txt"),
  },
  {
    // 2: With prompting. Download is renamed.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_PROMPT,

    EXPECTED_PROMPT("foo.txt"),
    PROMPT_RESPONSE("bar.txt"),

    INTERMEDIATE("bar.txt.intermediate"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("bar.txt.intermediate"),
    EXPECTED_FINAL("bar.txt"),
  },
  {
    // 3: With prompting. Download path is changed.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_PROMPT,

    EXPECTED_PROMPT("foo.txt"),
    PROMPT_RESPONSE("Foo/bar.txt"),

    INTERMEDIATE("Foo/bar.txt.intermediate"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("Foo/bar.txt.intermediate"),
    EXPECTED_FINAL("Foo/bar.txt"),
  },
  {
    // 4: No prompting. Intermediate path exists. Doesn't overwrite
    // intermediate path.
    TARGET("exists.png"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("exists.png.temp"),
    DONT_OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("exists.png (1).temp"),
    EXPECTED_FINAL("exists.png"),
  },
  {
    // 5: No prompting. Intermediate path exists. Overwrites.
    TARGET("exists.png"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("exists.png.temp"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("exists.png.temp"),
    EXPECTED_FINAL("exists.png"),
  },
  {
    // 6: No prompting. Final path exists. Doesn't overwrite.
    TARGET("exists.txt"),
    DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("exists.txt.temp"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("exists.txt.temp"),
    EXPECTED_FINAL("exists (1).txt"),
  },
  {
    // 7: No prompting. Final path exists. Overwrites.
    TARGET("exists.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("exists.txt.temp"),
    OVERWRITE,

    COMPLETES_AFTER_RENAME,
    EXPECTED_INTERMEDIATE("exists.txt.temp"),
    EXPECTED_FINAL("exists.txt"),
  },
  {
    // 8: No prompting. Response completes before filename determination.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECTED_PROMPT(""),
    PROMPT_RESPONSE(""),

    INTERMEDIATE("foo.txt.intermediate"),
    OVERWRITE,

    COMPLETES_BEFORE_RENAME,
    EXPECTED_INTERMEDIATE("foo.txt.intermediate"),
    EXPECTED_FINAL("foo.txt"),
  },
  {
    // 9: With prompting. Download path is changed. Response completes before
    // filename determination.
    TARGET("foo.txt"),
    DownloadItem::TARGET_DISPOSITION_PROMPT,

    EXPECTED_PROMPT("foo.txt"),
    PROMPT_RESPONSE("Foo/bar.txt"),

    INTERMEDIATE("Foo/bar.txt.intermediate"),
    OVERWRITE,

    COMPLETES_BEFORE_RENAME,
    EXPECTED_INTERMEDIATE("Foo/bar.txt.intermediate"),
    EXPECTED_FINAL("Foo/bar.txt"),
  },

#undef TARGET
#undef INTERMEDIATE
#undef EXPECTED_PROMPT
#undef PROMPT_RESPONSE
#undef EXPECTED_INTERMEDIATE
#undef EXPECTED_FINAL
};

} // namespace

TEST_F(DownloadManagerTest, DownloadFilenameTest) {
  ASSERT_TRUE(CreateTempDownloadsDirectory());

  // We create a known file to test file uniquification.
  ASSERT_TRUE(file_util::WriteFile(
      GetPathInDownloadsDir(FILE_PATH_LITERAL("exists.txt")), "", 0) == 0);
  ASSERT_TRUE(file_util::WriteFile(
      GetPathInDownloadsDir(FILE_PATH_LITERAL("exists.png.temp")), "", 0) == 0);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDownloadFilenameTestCases); ++i) {
    const DownloadFilenameTestCase& test_case = kDownloadFilenameTestCases[i];
    scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);

    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    info->url_chain.push_back(GURL());

    MockDownloadFile* download_file(
        new ::testing::StrictMock<MockDownloadFile>());
    FilePath target_path = GetPathInDownloadsDir(test_case.target_path);
    FilePath expected_prompt_path =
        GetPathInDownloadsDir(test_case.expected_prompt_path);
    FilePath chosen_path = GetPathInDownloadsDir(test_case.chosen_path);
    FilePath intermediate_path =
        GetPathInDownloadsDir(test_case.intermediate_path);
    FilePath expected_intermediate_path =
        GetPathInDownloadsDir(test_case.expected_intermediate_path);
    FilePath expected_final_path =
        GetPathInDownloadsDir(test_case.expected_final_path);

    EXPECT_CALL(*download_file, Rename(expected_intermediate_path))
        .WillOnce(Return(net::OK));
    EXPECT_CALL(*download_file, Rename(expected_final_path))
        .WillOnce(Return(net::OK));
#if defined(OS_MACOSX)
    EXPECT_CALL(*download_file, AnnotateWithSourceInformation());
#endif
    EXPECT_CALL(*download_file, Detach());

    download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
    AddMockDownloadToFileManager(info->download_id, download_file);
    DownloadItem* download = GetActiveDownloadItem(info->download_id);
    ASSERT_TRUE(download != NULL);

    if (test_case.completion == COMPLETES_BEFORE_RENAME)
      OnResponseCompleted(info->download_id, 1024, std::string("fake_hash"));

    download->OnTargetPathDetermined(
        target_path, test_case.target_disposition,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    download_manager_delegate_->SetFileSelectionExpectation(
        expected_prompt_path, chosen_path);
    download_manager_delegate_->SetIntermediatePath(
        intermediate_path, test_case.overwrite_intermediate_path);
    download_manager_delegate_->SetShouldCompleteDownload(false);
    download_manager_->RestartDownload(info->download_id.local());
    message_loop_.RunAllPending();

    if (test_case.completion == COMPLETES_AFTER_RENAME) {
      OnResponseCompleted(info->download_id, 1024, std::string("fake_hash"));
      message_loop_.RunAllPending();
    }

    EXPECT_EQ(expected_intermediate_path.value(),
              download->GetFullPath().value());
    download_manager_delegate_->SetShouldCompleteDownload(true);
    download_manager_delegate_->InvokeDownloadCompletionCallback();
    message_loop_.RunAllPending();
    EXPECT_EQ(expected_final_path.value(),
              download->GetTargetFilePath().value());
  }
}

void WriteCopyToStream(const char *source,
                     size_t len, content::ByteStreamWriter* stream) {
  scoped_refptr<net::IOBuffer> copy(new net::IOBuffer(len));
  memcpy(copy.get()->data(), source, len);
  stream->Write(copy, len);
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
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->total_bytes = static_cast<int64>(kTestDataLen);
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(GetTempDownloadPath(new_path));

  MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());

  EXPECT_CALL(*download_file, Rename(cr_path))
      .WillOnce(Return(net::OK));
  EXPECT_CALL(*download_file, Cancel());

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
  // |download_file| is owned by DownloadFileManager.
  AddMockDownloadToFileManager(info->download_id, download_file);

  DownloadItem* download = GetActiveDownloadItem(info->download_id);
  ASSERT_TRUE(download != NULL);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);

  int64 error_size = 3;
  OnDownloadInterrupted(info->download_id, error_size, "",
                        content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) == NULL);
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

  // Make sure the DFM exists; we'll need it later.
  // TODO(rdsmith): This is ugly and should be rewritten, but a large
  // rewrite of this test is in progress in a different CL, so doing it
  // the hacky way for now.
  (void) file_manager();

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  info->total_bytes = static_cast<int64>(kTestDataLen * 3);
  info->save_info.file_path = path;
  info->save_info.file_stream.reset(stream);
  scoped_ptr<content::ByteStreamWriter> stream_input;
  scoped_ptr<content::ByteStreamReader> stream_output;
  content::CreateByteStream(message_loop_.message_loop_proxy(),
                            message_loop_.message_loop_proxy(),
                            kTestDataLen, &stream_input, &stream_output);

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  // Create a download file that we can insert errors into.
  scoped_ptr<DownloadFileWithErrors> download_file(new DownloadFileWithErrors(
      info.get(), stream_output.Pass(), download_manager_, false));
  download_file->Initialize();

  DownloadItem* download = GetActiveDownloadItem(info->download_id);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Add some data before finalizing the file name.
  WriteCopyToStream(kTestData, kTestDataLen, stream_input.get());

  // Finalize the file name.
  ContinueDownloadWithPath(download, path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);

  // Add more data.
  WriteCopyToStream(kTestData, kTestDataLen, stream_input.get());

  // Add more data, but an error occurs.
  download_file->set_forced_error(net::ERR_FAILED);
  WriteCopyToStream(kTestData, kTestDataLen, stream_input.get());
  message_loop_.RunAllPending();

  // Check the state.  The download should have been interrupted.
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) == NULL);
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
  info->prompt_user_for_save_location = false;
  info->url_chain.push_back(GURL());
  const FilePath new_path(FILE_PATH_LITERAL("foo.zip"));
  const FilePath cr_path(GetTempDownloadPath(new_path));

  MockDownloadFile* download_file(new NiceMock<MockDownloadFile>());

  // |download_file| is owned by DownloadFileManager.
  EXPECT_CALL(*download_file, Rename(cr_path))
      .WillOnce(Return(net::OK));
  EXPECT_CALL(*download_file, Cancel());

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());
  AddMockDownloadToFileManager(info->download_id, download_file);

  DownloadItem* download = GetActiveDownloadItem(info->download_id);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);

  download->Cancel(false);
  message_loop_.RunAllPending();

  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);
  EXPECT_TRUE(observer->hit_state(DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(observer->hit_state(DownloadItem::CANCELLED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::INTERRUPTED));
  EXPECT_FALSE(observer->hit_state(DownloadItem::COMPLETE));
  EXPECT_FALSE(observer->hit_state(DownloadItem::REMOVING));
  EXPECT_TRUE(observer->was_updated());
  EXPECT_FALSE(observer->was_opened());
  EXPECT_FALSE(download->GetFileExternallyRemoved());
  EXPECT_EQ(DownloadItem::CANCELLED, download->GetState());

  file_manager()->CancelDownload(info->download_id);

  EXPECT_FALSE(file_util::PathExists(new_path));
  EXPECT_FALSE(file_util::PathExists(cr_path));
}

TEST_F(DownloadManagerTest, MAYBE_DownloadOverwriteTest) {
  using ::testing::_;
  using ::testing::CreateFunctor;
  using ::testing::Invoke;
  using ::testing::Return;

  ASSERT_TRUE(CreateTempDownloadsDirectory());
  // File names we're using.
  const FilePath new_path(GetPathInDownloadsDir(FILE_PATH_LITERAL("foo.txt")));
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
  info->prompt_user_for_save_location = true;
  info->url_chain.push_back(GURL());
  scoped_ptr<content::ByteStreamWriter> stream_input;
  scoped_ptr<content::ByteStreamReader> stream_output;
  content::CreateByteStream(message_loop_.message_loop_proxy(),
                            message_loop_.message_loop_proxy(),
                            kTestDataLen, &stream_input, &stream_output);

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(info->download_id);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), stream_output.Pass(),
                           new DownloadRequestHandle(),
                           download_manager_, false,
                           scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                           net::BoundNetLog()));
  download_file->Rename(cr_path);
  // This creates the .temp version of the file.
  download_file->Initialize();
  // |download_file| is owned by DownloadFileManager.
  AddDownloadToFileManager(info->download_id, download_file);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);

  WriteCopyToStream(kTestData, kTestDataLen, stream_input.get());

  // Finish the download.
  OnResponseCompleted(info->download_id, kTestDataLen, "");
  message_loop_.RunAllPending();

  // Download is complete.
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) == NULL);
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

  ASSERT_TRUE(CreateTempDownloadsDirectory());

  // File names we're using.
  const FilePath new_path(GetPathInDownloadsDir(FILE_PATH_LITERAL("foo.txt")));
  const FilePath cr_path(GetTempDownloadPath(new_path));
  EXPECT_FALSE(file_util::PathExists(new_path));

  // Normally, the download system takes ownership of info, and is
  // responsible for deleting it.  In these unit tests, however, we
  // don't call the function that deletes it, so we do so ourselves.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  info->prompt_user_for_save_location = true;
  info->url_chain.push_back(GURL());
  scoped_ptr<content::ByteStreamWriter> stream_input;
  scoped_ptr<content::ByteStreamReader> stream_output;
  content::CreateByteStream(message_loop_.message_loop_proxy(),
                            message_loop_.message_loop_proxy(),
                            kTestDataLen, &stream_input, &stream_output);

  download_manager_->CreateDownloadItem(info.get(), DownloadRequestHandle());

  DownloadItem* download = GetActiveDownloadItem(info->download_id);
  ASSERT_TRUE(download != NULL);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  scoped_ptr<ItemObserver> observer(new ItemObserver(download));

  // Create and initialize the download file.  We're bypassing the first part
  // of the download process and skipping to the part after the final file
  // name has been chosen, so we need to initialize the download file
  // properly.
  DownloadFile* download_file(
      new DownloadFileImpl(info.get(), stream_output.Pass(),
                           new DownloadRequestHandle(),
                           download_manager_, false,
                           scoped_ptr<PowerSaveBlocker>(NULL).Pass(),
                           net::BoundNetLog()));
  download_file->Rename(cr_path);
  // This creates the .temp version of the file.
  download_file->Initialize();
  // |download_file| is owned by DownloadFileManager.
  AddDownloadToFileManager(info->download_id, download_file);

  ContinueDownloadWithPath(download, new_path);
  message_loop_.RunAllPending();
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) != NULL);

  WriteCopyToStream(kTestData, kTestDataLen, stream_input.get());

  // Finish the download.
  OnResponseCompleted(info->download_id, kTestDataLen, "");
  message_loop_.RunAllPending();

  // Download is complete.
  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) == NULL);
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

  EXPECT_TRUE(GetActiveDownloadItem(info->download_id) == NULL);
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
