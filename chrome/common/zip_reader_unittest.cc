// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/zip_reader.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <set>
#include <string>

#include "base/file_util.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/zip_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

#if defined(OS_POSIX)
// Wrap file descriptors in a class so that we don't leak them in tests.
class FdWrapper {
 public:
  typedef enum {
    READ_ONLY,
    READ_WRITE
  } AccessMode;

  FdWrapper(const FilePath& file, AccessMode mode) : fd_(-1) {
    switch (mode) {
    case READ_ONLY:
      fd_ = open(file.value().c_str(), O_RDONLY);
      break;
    case READ_WRITE:
      fd_ = open(file.value().c_str(),
                 O_RDWR | O_CREAT,
                 S_IRUSR | S_IWUSR);
      break;
    default:
      NOTREACHED();
    }
    return;
  }

  ~FdWrapper() {
    close(fd_);
  }

  int fd() { return fd_; }

 private:
  int fd_;
};
#endif

}   // namespace

namespace zip {

// Make the test a PlatformTest to setup autorelease pools properly on Mac.
class ZipReaderTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.path();

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("zip");

    test_zip_file_ = test_data_dir_.AppendASCII("test.zip");
    evil_zip_file_ = test_data_dir_.AppendASCII("evil.zip");
    evil_via_invalid_utf8_zip_file_ = test_data_dir_.AppendASCII(
        "evil_via_invalid_utf8.zip");
    evil_via_absolute_file_name_zip_file_ = test_data_dir_.AppendASCII(
        "evil_via_absolute_file_name.zip");

    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/bar/")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/bar/baz.txt")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/bar/quux.txt")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/bar.txt")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo.txt")));
    test_zip_contents_.insert(FilePath(FILE_PATH_LITERAL("foo/bar/.hidden")));
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
  }

  // The path to temporary directory used to contain the test operations.
  FilePath test_dir_;
  // The path to the test data directory where test.zip etc. are located.
  FilePath test_data_dir_;
  // The path to test.zip in the test data directory.
  FilePath test_zip_file_;
  // The path to evil.zip in the test data directory.
  FilePath evil_zip_file_;
  // The path to evil_via_invalid_utf8.zip in the test data directory.
  FilePath evil_via_invalid_utf8_zip_file_;
  // The path to evil_via_absolute_file_name.zip in the test data directory.
  FilePath evil_via_absolute_file_name_zip_file_;
  std::set<FilePath> test_zip_contents_;

  ScopedTempDir temp_dir_;
};

TEST_F(ZipReaderTest, Open_ValidZipFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
}

#if defined(OS_POSIX)
TEST_F(ZipReaderTest, Open_ValidZipFd) {
  ZipReader reader;
  FdWrapper zip_fd_wrapper(test_zip_file_, FdWrapper::READ_ONLY);
  ASSERT_TRUE(reader.OpenFromFd(zip_fd_wrapper.fd()));
}
#endif

TEST_F(ZipReaderTest, Open_NonExistentFile) {
  ZipReader reader;
  ASSERT_FALSE(reader.Open(test_data_dir_.AppendASCII("nonexistent.zip")));
}

TEST_F(ZipReaderTest, Open_ExistentButNonZipFile) {
  ZipReader reader;
  ASSERT_FALSE(reader.Open(test_data_dir_.AppendASCII("create_test_zip.sh")));
}

// Iterate through the contents in the test zip file, and compare that the
// contents collected from the zip reader matches the expected contents.
TEST_F(ZipReaderTest, Iteration) {
  std::set<FilePath> actual_contents;
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  while (reader.HasMore()) {
    ASSERT_TRUE(reader.OpenCurrentEntryInZip());
    actual_contents.insert(reader.current_entry_info()->file_path());
    ASSERT_TRUE(reader.AdvanceToNextEntry());
  }
  EXPECT_FALSE(reader.AdvanceToNextEntry());  // Shouldn't go further.
  EXPECT_EQ(test_zip_contents_.size(),
            static_cast<size_t>(reader.num_entries()));
  EXPECT_EQ(test_zip_contents_.size(), actual_contents.size());
  EXPECT_EQ(test_zip_contents_, actual_contents);
}

#if defined(OS_POSIX)
// Open the test zip file from a file descriptor, iterate through its contents,
// and compare that they match the expected contents.
TEST_F(ZipReaderTest, FdIteration) {
  std::set<FilePath> actual_contents;
  ZipReader reader;
  FdWrapper zip_fd_wrapper(test_zip_file_, FdWrapper::READ_ONLY);
  ASSERT_TRUE(reader.OpenFromFd(zip_fd_wrapper.fd()));
  while (reader.HasMore()) {
    ASSERT_TRUE(reader.OpenCurrentEntryInZip());
    actual_contents.insert(reader.current_entry_info()->file_path());
    ASSERT_TRUE(reader.AdvanceToNextEntry());
  }
  EXPECT_FALSE(reader.AdvanceToNextEntry());  // Shouldn't go further.
  EXPECT_EQ(test_zip_contents_.size(),
            static_cast<size_t>(reader.num_entries()));
  EXPECT_EQ(test_zip_contents_.size(), actual_contents.size());
  EXPECT_EQ(test_zip_contents_, actual_contents);
}
#endif

TEST_F(ZipReaderTest, LocateAndOpenEntry_ValidFile) {
  std::set<FilePath> actual_contents;
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  EXPECT_EQ(target_path, reader.current_entry_info()->file_path());
}

TEST_F(ZipReaderTest, LocateAndOpenEntry_NonExistentFile) {
  std::set<FilePath> actual_contents;
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("nonexistent.txt"));
  ASSERT_FALSE(reader.LocateAndOpenEntry(target_path));
  EXPECT_EQ(NULL, reader.current_entry_info());
}

TEST_F(ZipReaderTest, ExtractCurrentEntryToFilePath_RegularFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ASSERT_TRUE(reader.ExtractCurrentEntryToFilePath(
      test_dir_.AppendASCII("quux.txt")));
  // Read the output file ans compute the MD5.
  std::string output;
  ASSERT_TRUE(file_util::ReadFileToString(test_dir_.AppendASCII("quux.txt"),
                                          &output));
  const std::string md5 = base::MD5String(output);
  const std::string kExpectedMD5 = "d1ae4ac8a17a0e09317113ab284b57a6";
  EXPECT_EQ(kExpectedMD5, md5);
  // quux.txt should be larger than kZipBufSize so that we can exercise
  // the loop in ExtractCurrentEntry().
  EXPECT_LT(static_cast<size_t>(internal::kZipBufSize), output.size());
}

#if defined(OS_POSIX)
TEST_F(ZipReaderTest, FdExtractCurrentEntryToFilePath_RegularFile) {
  ZipReader reader;
  FdWrapper zip_fd_wrapper(test_zip_file_, FdWrapper::READ_ONLY);
  ASSERT_TRUE(reader.OpenFromFd(zip_fd_wrapper.fd()));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ASSERT_TRUE(reader.ExtractCurrentEntryToFilePath(
      test_dir_.AppendASCII("quux.txt")));
  // Read the output file and compute the MD5.
  std::string output;
  ASSERT_TRUE(file_util::ReadFileToString(test_dir_.AppendASCII("quux.txt"),
                                          &output));
  const std::string md5 = base::MD5String(output);
  const std::string kExpectedMD5 = "d1ae4ac8a17a0e09317113ab284b57a6";
  EXPECT_EQ(kExpectedMD5, md5);
  // quux.txt should be larger than kZipBufSize so that we can exercise
  // the loop in ExtractCurrentEntry().
  EXPECT_LT(static_cast<size_t>(internal::kZipBufSize), output.size());
}

TEST_F(ZipReaderTest, FdExtractCurrentEntryToFd_RegularFile) {
  ZipReader reader;
  FdWrapper zip_fd_wrapper(test_zip_file_, FdWrapper::READ_ONLY);
  ASSERT_TRUE(reader.OpenFromFd(zip_fd_wrapper.fd()));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  FilePath out_path = test_dir_.AppendASCII("quux.txt");
  FdWrapper out_fd_w(out_path, FdWrapper::READ_WRITE);
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ASSERT_TRUE(reader.ExtractCurrentEntryToFd(out_fd_w.fd()));
  // Read the output file and compute the MD5.
  std::string output;
  ASSERT_TRUE(file_util::ReadFileToString(test_dir_.AppendASCII("quux.txt"),
                                          &output));
  const std::string md5 = base::MD5String(output);
  const std::string kExpectedMD5 = "d1ae4ac8a17a0e09317113ab284b57a6";
  EXPECT_EQ(kExpectedMD5, md5);
  // quux.txt should be larger than kZipBufSize so that we can exercise
  // the loop in ExtractCurrentEntry().
  EXPECT_LT(static_cast<size_t>(internal::kZipBufSize), output.size());
}
#endif

TEST_F(ZipReaderTest, ExtractCurrentEntryToFilePath_Directory) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ASSERT_TRUE(reader.ExtractCurrentEntryToFilePath(
      test_dir_.AppendASCII("foo")));
  // The directory should be created.
  ASSERT_TRUE(file_util::DirectoryExists(test_dir_.AppendASCII("foo")));
}

TEST_F(ZipReaderTest, ExtractCurrentEntryIntoDirectory_RegularFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ASSERT_TRUE(reader.ExtractCurrentEntryIntoDirectory(test_dir_));
  // Sub directories should be created.
  ASSERT_TRUE(file_util::DirectoryExists(test_dir_.AppendASCII("foo/bar")));
  // And the file should be created.
  std::string output;
  ASSERT_TRUE(file_util::ReadFileToString(
      test_dir_.AppendASCII("foo/bar/quux.txt"), &output));
  const std::string md5 = base::MD5String(output);
  const std::string kExpectedMD5 = "d1ae4ac8a17a0e09317113ab284b57a6";
  EXPECT_EQ(kExpectedMD5, md5);
}

TEST_F(ZipReaderTest, current_entry_info_RegularFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/quux.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ZipReader::EntryInfo* current_entry_info = reader.current_entry_info();

  EXPECT_EQ(target_path, current_entry_info->file_path());
  EXPECT_EQ(13527, current_entry_info->original_size());

  // The expected time stamp: 2009-05-29 06:22:20
  base::Time::Exploded exploded = {};  // Zero-clear.
  current_entry_info->last_modified().LocalExplode(&exploded);
  EXPECT_EQ(2009, exploded.year);
  EXPECT_EQ(5, exploded.month);
  EXPECT_EQ(29, exploded.day_of_month);
  EXPECT_EQ(6, exploded.hour);
  EXPECT_EQ(22, exploded.minute);
  EXPECT_EQ(20, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  EXPECT_FALSE(current_entry_info->is_unsafe());
  EXPECT_FALSE(current_entry_info->is_directory());
}

TEST_F(ZipReaderTest, current_entry_info_DotDotFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(evil_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL(
      "../levilevilevilevilevilevilevilevilevilevilevilevil"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ZipReader::EntryInfo* current_entry_info = reader.current_entry_info();
  EXPECT_EQ(target_path, current_entry_info->file_path());

  // This file is unsafe because of ".." in the file name.
  EXPECT_TRUE(current_entry_info->is_unsafe());
  EXPECT_FALSE(current_entry_info->is_directory());
}

TEST_F(ZipReaderTest, current_entry_info_InvalidUTF8File) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(evil_via_invalid_utf8_zip_file_));
  // The evil file is the 2nd file in the zip file.
  // We cannot locate by the file name ".\x80.\\evil.txt",
  // as FilePath may internally convert the string.
  ASSERT_TRUE(reader.AdvanceToNextEntry());
  ASSERT_TRUE(reader.OpenCurrentEntryInZip());
  ZipReader::EntryInfo* current_entry_info = reader.current_entry_info();

  // This file is unsafe because of invalid UTF-8 in the file name.
  EXPECT_TRUE(current_entry_info->is_unsafe());
  EXPECT_FALSE(current_entry_info->is_directory());
}

TEST_F(ZipReaderTest, current_entry_info_AbsoluteFile) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(evil_via_absolute_file_name_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("/evil.txt"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ZipReader::EntryInfo* current_entry_info = reader.current_entry_info();
  EXPECT_EQ(target_path, current_entry_info->file_path());

  // This file is unsafe because of the absolute file name.
  EXPECT_TRUE(current_entry_info->is_unsafe());
  EXPECT_FALSE(current_entry_info->is_directory());
}

TEST_F(ZipReaderTest, current_entry_info_Directory) {
  ZipReader reader;
  ASSERT_TRUE(reader.Open(test_zip_file_));
  FilePath target_path(FILE_PATH_LITERAL("foo/bar/"));
  ASSERT_TRUE(reader.LocateAndOpenEntry(target_path));
  ZipReader::EntryInfo* current_entry_info = reader.current_entry_info();

  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("foo/bar/")),
            current_entry_info->file_path());
  // The directory size should be zero.
  EXPECT_EQ(0, current_entry_info->original_size());

  // The expected time stamp: 2009-05-31 15:49:52
  base::Time::Exploded exploded = {};  // Zero-clear.
  current_entry_info->last_modified().LocalExplode(&exploded);
  EXPECT_EQ(2009, exploded.year);
  EXPECT_EQ(5, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(15, exploded.hour);
  EXPECT_EQ(49, exploded.minute);
  EXPECT_EQ(52, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  EXPECT_FALSE(current_entry_info->is_unsafe());
  EXPECT_TRUE(current_entry_info->is_directory());
}

}  // namespace zip
