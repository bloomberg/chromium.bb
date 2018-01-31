// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

namespace {

scoped_refptr<ContentHashReader> CreateContentHashReader(
    const Extension& extension,
    const base::FilePath& extension_resource_path) {
  return base::MakeRefCounted<ContentHashReader>(
      extension.id(), extension.version(), extension.path(),
      extension_resource_path,
      ContentVerifierKey(kWebstoreSignaturesPublicKey,
                         kWebstoreSignaturesPublicKeySize));
}

void DoNothingWithReasonParam(ContentVerifyJob::FailureReason reason) {}

}  // namespace

class ContentVerifyJobUnittest : public ExtensionsTest {
 public:
  ContentVerifyJobUnittest()
      // The TestBrowserThreadBundle is needed for ContentVerifyJob::Start().
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>(
            content::TestBrowserThreadBundle::REAL_IO_THREAD)) {}
  ~ContentVerifyJobUnittest() override {}

  // Helper to get files from our subdirectory in the general extensions test
  // data dir.
  base::FilePath GetTestPath(const base::FilePath& relative_path) {
    base::FilePath base_path;
    EXPECT_TRUE(PathService::Get(DIR_TEST_DATA, &base_path));
    base_path = base_path.AppendASCII("content_hash_fetcher");
    return base_path.Append(relative_path);
  }

 protected:
  ContentVerifyJob::FailureReason RunContentVerifyJob(
      const Extension& extension,
      const base::FilePath& resource_path,
      std::string& resource_contents) {
    TestContentVerifyJobObserver observer(extension.id(), resource_path);
    scoped_refptr<ContentHashReader> content_hash_reader =
        CreateContentHashReader(extension, resource_path);
    scoped_refptr<ContentVerifyJob> verify_job = new ContentVerifyJob(
        content_hash_reader.get(), base::Bind(&DoNothingWithReasonParam));
    verify_job->Start();
    {
      // Simulate serving |resource_contents| from |resource_path|.
      verify_job->BytesRead(resource_contents.size(),
                            base::string_as_array(&resource_contents));
      verify_job->DoneReading();
    }
    return observer.WaitAndGetFailureReason();
  }

  // Runs test to verify that a modified extension resource (background.js)
  // causes ContentVerifyJob to fail with HASH_MISMATCH. The string
  // |content_to_append_for_mismatch| is appended to the resource for
  // modification.
  void RunContentMismatchTest(
      const std::string& content_to_append_for_mismatch) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath unzipped_path = temp_dir_.GetPath();
    base::FilePath test_dir_base = GetTestPath(
        base::FilePath(FILE_PATH_LITERAL("with_verified_contents")));
    scoped_refptr<Extension> extension =
        content_verifier_test_utils::UnzipToDirAndLoadExtension(
            test_dir_base.AppendASCII("source_all.zip"), unzipped_path);
    ASSERT_TRUE(extension.get());
    // Make sure there is a verified_contents.json file there as this test
    // cannot fetch it.
    EXPECT_TRUE(base::PathExists(
        file_util::GetVerifiedContentsPath(extension->path())));

    const base::FilePath::CharType kResource[] =
        FILE_PATH_LITERAL("background.js");
    base::FilePath existent_resource_path(kResource);
    {
      // Make sure modified background.js fails content verification.
      std::string modified_contents;
      base::ReadFileToString(unzipped_path.Append(base::FilePath(kResource)),
                             &modified_contents);
      modified_contents.append(content_to_append_for_mismatch);
      EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
                RunContentVerifyJob(*extension.get(), existent_resource_path,
                                    modified_contents));
    }
  }

  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentVerifyJobUnittest);
};

// Tests that deleted legitimate files trigger content verification failure.
// Also tests that non-existent file request does not trigger content
// verification failure.
TEST_F(ContentVerifyJobUnittest, DeletedAndMissingFiles) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir_.GetPath();
  base::FilePath test_dir_base =
      GetTestPath(base::FilePath(FILE_PATH_LITERAL("with_verified_contents")));
  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          test_dir_base.AppendASCII("source_all.zip"), unzipped_path);
  ASSERT_TRUE(extension.get());
  // Make sure there is a verified_contents.json file there as this test cannot
  // fetch it.
  EXPECT_TRUE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));

  const base::FilePath::CharType kExistentResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath existent_resource_path(kExistentResource);
  {
    // Make sure background.js passes verification correctly.
    std::string contents;
    base::ReadFileToString(
        unzipped_path.Append(base::FilePath(kExistentResource)), &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  contents));
  }

  {
    // Once background.js is deleted, verification will result in HASH_MISMATCH.
    // Delete the existent file first.
    EXPECT_TRUE(base::DeleteFile(
        unzipped_path.Append(base::FilePath(kExistentResource)), false));

    // Deleted file will serve empty contents.
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), existent_resource_path,
                                  empty_contents));
  }

  {
    // Now ask for a non-existent resource non-existent.js. Verification should
    // skip this file as it is not listed in our verified_contents.json file.
    const base::FilePath::CharType kNonExistentResource[] =
        FILE_PATH_LITERAL("non-existent.js");
    base::FilePath non_existent_resource_path(kNonExistentResource);
    // Non-existent file will serve empty contents.
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), non_existent_resource_path,
                                  empty_contents));
  }

  {
    // Now create a resource foo.js which exists on disk but is not in the
    // extension's verified_contents.json. Verification should result in
    // NO_HASHES_FOR_FILE since the extension is trying to load a file the
    // extension should not have.
    const base::FilePath::CharType kUnexpectedResource[] =
        FILE_PATH_LITERAL("foo.js");
    base::FilePath unexpected_resource_path(kUnexpectedResource);

    base::FilePath full_path =
        unzipped_path.Append(base::FilePath(unexpected_resource_path));
    base::WriteFile(full_path, "42", sizeof("42"));

    std::string contents;
    base::ReadFileToString(full_path, &contents);
    EXPECT_EQ(ContentVerifyJob::NO_HASHES_FOR_FILE,
              RunContentVerifyJob(*extension.get(), unexpected_resource_path,
                                  contents));
  }

  {
    // Ask for the root path of the extension (i.e., chrome-extension://<id>/).
    // Verification should skip this request as if the resource were
    // non-existent. See https://crbug.com/791929.
    base::FilePath empty_path_resource_path(FILE_PATH_LITERAL(""));
    std::string empty_contents;
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), empty_path_resource_path,
                                  empty_contents));
  }
}

// Tests that content modification causes content verification failure.
TEST_F(ContentVerifyJobUnittest, ContentMismatch) {
  RunContentMismatchTest("console.log('modified');");
}

// Similar to ContentMismatch, but uses a file size > 4k.
// Regression test for https://crbug.com/804630.
TEST_F(ContentVerifyJobUnittest, ContentMismatchWithLargeFile) {
  std::string content_larger_than_block_size(
      extension_misc::kContentVerificationDefaultBlockSize + 1, ';');
  RunContentMismatchTest(content_larger_than_block_size);
}

// Tests that extension resources that are originally 0 byte behave correctly
// with content verification.
TEST_F(ContentVerifyJobUnittest, LegitimateZeroByteFile) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir_.GetPath();
  base::FilePath test_dir_base =
      GetTestPath(base::FilePath(FILE_PATH_LITERAL("zero_byte_file")));
  // |extension| has a 0 byte background.js file in it.
  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          test_dir_base.AppendASCII("source.zip"), unzipped_path);
  ASSERT_TRUE(extension.get());
  // Make sure there is a verified_contents.json file there as this test cannot
  // fetch it.
  EXPECT_TRUE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));

  const base::FilePath::CharType kResource[] =
      FILE_PATH_LITERAL("background.js");
  base::FilePath resource_path(kResource);
  {
    // Make sure 0 byte background.js passes content verification.
    std::string contents;
    base::ReadFileToString(unzipped_path.Append(base::FilePath(kResource)),
                           &contents);
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), resource_path, contents));
  }

  {
    // Make sure non-empty background.js fails content verification.
    std::string modified_contents = "console.log('non empty');";
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH,
              RunContentVerifyJob(*extension.get(), resource_path,
                                  modified_contents));
  }
}

// Tests that extension resources of different interesting sizes work properly.
// Regression test for https://crbug.com/720597, where content verification
// always failed for sizes multiple of content hash's block size (4096 bytes).
TEST_F(ContentVerifyJobUnittest, DifferentSizedFiles) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir_.GetPath();
  base::FilePath test_dir_base =
      GetTestPath(base::FilePath(FILE_PATH_LITERAL("different_sized_files")));
  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          test_dir_base.AppendASCII("source.zip"), unzipped_path);
  ASSERT_TRUE(extension.get());
  // Make sure there is a verified_contents.json file there as this test cannot
  // fetch it.
  EXPECT_TRUE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));

  const struct {
    const char* name;
    size_t byte_size;
  } kFilesToTest[] = {
      {"1024.js", 1024}, {"4096.js", 4096}, {"8192.js", 8192},
      {"8191.js", 8191}, {"8193.js", 8193},
  };
  for (const auto& file_to_test : kFilesToTest) {
    base::FilePath resource_path =
        base::FilePath::FromUTF8Unsafe(file_to_test.name);
    std::string contents;
    base::ReadFileToString(unzipped_path.AppendASCII(file_to_test.name),
                           &contents);
    EXPECT_EQ(file_to_test.byte_size, contents.size());
    EXPECT_EQ(ContentVerifyJob::NONE,
              RunContentVerifyJob(*extension.get(), resource_path, contents));
  }
}

}  // namespace extensions
