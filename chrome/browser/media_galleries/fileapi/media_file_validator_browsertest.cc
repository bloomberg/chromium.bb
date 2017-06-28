// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/fileapi/copy_or_move_file_validator.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "http://foo";

const char kValidImage[] = "RIFF0\0\0\0WEBPVP8 $\0\0\0\xB2\x02\0\x9D\x01\x2A"
                           "\x01\0\x01\0\x2F\x9D\xCE\xE7s\xA8((((\x01\x9CK(\0"
                           "\x05\xCE\xB3l\0\0\xFE\xD8\x80\0\0";

const char kInvalidMediaFile[] = "Not a media file";

const int64_t kNoFileSize = -1;

void HandleCheckFileResult(int64_t expected_size,
                           const base::Callback<void(bool success)>& callback,
                           base::File::Error result,
                           const base::File::Info& file_info) {
  if (result == base::File::FILE_OK) {
    if (!file_info.is_directory && expected_size != kNoFileSize &&
        file_info.size == expected_size) {
      callback.Run(true);
      return;
    }
  } else {
    if (expected_size == kNoFileSize) {
      callback.Run(true);
      return;
    }
  }
  callback.Run(false);
}

base::FilePath GetMediaTestDir() {
  base::FilePath test_file;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &test_file))
    return base::FilePath();
  return test_file.AppendASCII("media").AppendASCII("test").AppendASCII("data");
}

}  // namespace

class MediaFileValidatorTest : public InProcessBrowserTest {
 public:
  MediaFileValidatorTest() : test_file_size_(0) {}

  ~MediaFileValidatorTest() override {}

  void TearDownOnMainThread() override {
    // Release our ref to |file_system_context_| before the test framework winds
    // down, otherwise releasing it in the destructor posts a destruction task
    // to the FILE thread after it has been shutdown (which base/task_scheduler
    // guards against in the RedirectNonUINonIOBrowserThreads experiment per the
    // FILE thread's tasks being marked as shutdown blocking for legacy
    // reasons).
    file_system_context_ = nullptr;
  }

  // Write |content| into |filename| in a test file system and try to move
  // it into a media file system.  The result is compared to |expected_result|.
  void MoveTest(const std::string& filename, const std::string& content,
                bool expected_result) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::BindOnce(&MediaFileValidatorTest::SetupOnFileThread,
                       base::Unretained(this), filename, content,
                       expected_result));
    loop_runner_ = new content::MessageLoopRunner;
    loop_runner_->Run();
  }

  // Write |source| into |filename| in a test file system and try to move it
  // into a media file system.  The result is compared to |expected_result|.
  void MoveTestFromFile(const std::string& filename,
                        const base::FilePath& source, bool expected_result) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::BindOnce(&MediaFileValidatorTest::SetupFromFileOnFileThread,
                       base::Unretained(this), filename, source,
                       expected_result));
    loop_runner_ = new content::MessageLoopRunner;
    loop_runner_->Run();
  }

 private:
  // Create the test files, filesystem objects, etc.
  void SetupOnFileThread(const std::string& filename,
                         const std::string& content,
                         bool expected_result) {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    base::FilePath base = base_dir_.GetPath();
    base::FilePath src_path = base.AppendASCII("src_fs");
    ASSERT_TRUE(base::CreateDirectory(src_path));

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(
        base::MakeUnique<content::TestFileSystemBackend>(
            base::ThreadTaskRunnerHandle::Get().get(), src_path));
    additional_providers.push_back(base::MakeUnique<MediaFileSystemBackend>(
        base, base::ThreadTaskRunnerHandle::Get().get()));
    file_system_context_ =
        content::CreateFileSystemContextWithAdditionalProvidersForTesting(
            NULL, std::move(additional_providers), base);

    move_src_ = file_system_context_->CreateCrackedFileSystemURL(
        GURL(kOrigin),
        storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(filename));

    test_file_size_ = content.size();
    base::FilePath test_file = src_path.AppendASCII(filename);
    ASSERT_EQ(test_file_size_,
              base::WriteFile(test_file, content.data(), test_file_size_));

    base::FilePath dest_path = base.AppendASCII("dest_fs");
    ASSERT_TRUE(base::CreateDirectory(dest_path));
    std::string dest_fsid =
        storage::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
            storage::kFileSystemTypeNativeMedia,
            std::string(),
            dest_path,
            NULL);

    size_t extension_index = filename.find_last_of(".");
    ASSERT_NE(std::string::npos, extension_index);
    std::string extension = filename.substr(extension_index);
    std::string dest_root_fs_url = storage::GetIsolatedFileSystemRootURIString(
        GURL(kOrigin), dest_fsid, "dest_fs/");
    move_dest_ = file_system_context_->CrackURL(GURL(
          dest_root_fs_url + "move_dest" + extension));

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&MediaFileValidatorTest::CheckFiles,
                       base::Unretained(this), true,
                       base::Bind(&MediaFileValidatorTest::OnTestFilesReady,
                                  base::Unretained(this), expected_result)));
  }

  void SetupFromFileOnFileThread(const std::string& filename,
                                 const base::FilePath& source,
                                 bool expected_result) {
    std::string content;
    ASSERT_TRUE(base::ReadFileToString(source, &content));
    SetupOnFileThread(filename, content, expected_result);
  }

  // Check that exactly one of |move_src_| and |move_dest_| exists.
  // |src_expected| indicates which one should exist.  When complete,
  // |callback| is called with success/failure.
  void CheckFiles(bool src_expected,
                  const base::Callback<void(bool success)>& callback) {
    CheckFile(move_src_, src_expected ? test_file_size_ : kNoFileSize,
              base::Bind(&MediaFileValidatorTest::OnCheckFilesFirstResult,
                         base::Unretained(this), !src_expected, callback));
  }

  // Helper that checks a file has the |expected_size|, which may be
  // |kNoFileSize| if the file should not exist.  |callback| is called
  // with success/failure.
  void CheckFile(storage::FileSystemURL url,
                 int64_t expected_size,
                 const base::Callback<void(bool success)>& callback) {
    operation_runner()->GetMetadata(
        url, storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
        base::Bind(&HandleCheckFileResult, expected_size, callback));
  }

  // Helper that checks the result of |move_src_| lookup and then checks
  // |move_dest_| if all is as expected.
  void OnCheckFilesFirstResult(bool dest_expected,
                               const base::Callback<void(bool)>& callback,
                               bool src_result) {
    EXPECT_TRUE(src_result);
    if (!src_result) {
      callback.Run(false);
      return;
    }
    CheckFile(move_dest_, dest_expected ? test_file_size_ : kNoFileSize,
              callback);
  }

  // Assert |test_files_ready| and then do the actual test of moving
  // |move_src_| to |move_dest_|.
  void OnTestFilesReady(bool expected_result, bool test_files_ready) {
    ASSERT_TRUE(test_files_ready);
    operation_runner()->Move(move_src_,
                             move_dest_,
                             storage::FileSystemOperation::OPTION_NONE,
                             base::Bind(&MediaFileValidatorTest::OnMoveResult,
                                        base::Unretained(this),
                                        expected_result));
  }

  // Check that the move succeeded/failed based on expectation and then
  // check that the right file exists.
  void OnMoveResult(bool expected_result, base::File::Error result) {
    if (expected_result)
      EXPECT_EQ(base::File::FILE_OK, result);
    else
      EXPECT_EQ(base::File::FILE_ERROR_SECURITY, result);
    CheckFiles(!expected_result,
               base::Bind(&MediaFileValidatorTest::OnTestFilesCheckResult,
                          base::Unretained(this)));
  }

  // Check that the correct test file exists and then post the result back
  // to the UI thread.
  void OnTestFilesCheckResult(bool result) {
    EXPECT_TRUE(result);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     loop_runner_->QuitClosure());
  }

  storage::FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

  base::ScopedTempDir base_dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  int test_file_size_;

  storage::FileSystemURL move_src_;
  storage::FileSystemURL move_dest_;

  scoped_refptr<content::MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileValidatorTest);
};

// Flaky on linux_chromium_rel_ng. https://crbug.com/704614.
#if defined(OS_LINUX)
#define MAYBE_UnsupportedExtension DISABLED_UnsupportedExtension
#else
#define MAYBE_UnsupportedExtension UnsupportedExtension
#endif
IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, MAYBE_UnsupportedExtension) {
  MoveTest("a.txt", std::string(kValidImage, arraysize(kValidImage)), false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, ValidImage) {
  MoveTest("a.webp", std::string(kValidImage, arraysize(kValidImage)), true);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidImage) {
  MoveTest("a.webp", std::string(kInvalidMediaFile,
           arraysize(kInvalidMediaFile)), false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidAudio) {
  MoveTest("a.ogg", std::string(kInvalidMediaFile,
           arraysize(kInvalidMediaFile)), false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, ValidAudio) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("sfx.ogg");
  MoveTestFromFile("sfx.ogg", test_file, true);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidVideo) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("no_streams.webm");
  MoveTestFromFile("no_streams.webm", test_file, false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, ValidVideo) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("bear-320x240-multitrack.webm");
  MoveTestFromFile("multitrack.webm", test_file, true);
}
