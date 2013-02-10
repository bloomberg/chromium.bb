// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_change.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drive {

TEST(FileChangeTest, Getters) {
  base::FilePath change_path(FILE_PATH_LITERAL("test"));
  FileChange::Type change_type = FileChange::ADDED;

  FileChange file_change(change_path, change_type);

  EXPECT_EQ(change_path, file_change.path());
  EXPECT_EQ(change_type, file_change.type());
}

TEST(FileChangeTest, FactoryMethod) {
  base::FilePath change_path(FILE_PATH_LITERAL("a/b/c/d"));
  FileChange::Type change_type = FileChange::CHANGED;

  FileChangeSet changed_files =
      FileChange::CreateSingleSet(change_path, change_type);

  ASSERT_EQ(1u, changed_files.size());
  FileChangeSet::iterator it = changed_files.begin();

  EXPECT_EQ(change_path, it->path());
  EXPECT_EQ(change_type, it->type());
}

TEST(FileChangeTest, Equals) {
  // Change is equal if and only if both path and type are equal.
  // Paths differ, types are equal.
  FileChange file_change1(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::ADDED);
  FileChange file_change2(base::FilePath(FILE_PATH_LITERAL("b")),
                          FileChange::ADDED);
  EXPECT_EQ(file_change1, file_change1);
  EXPECT_FALSE(file_change1 == file_change2);

  // Paths are equal, types differ.
  FileChange file_change3(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::DELETED);
  FileChange file_change4(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::CHANGED);
  EXPECT_FALSE(file_change3 == file_change4);

  // Paths and types are equal.
  FileChange file_change5(base::FilePath(FILE_PATH_LITERAL("c")),
                          FileChange::ADDED);
  FileChange file_change6(base::FilePath(FILE_PATH_LITERAL("c")),
                          FileChange::ADDED);
  EXPECT_EQ(file_change5, file_change6);
}

TEST(FileChangeTest, Compare) {
  FileChange file_change1(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::DELETED);
  FileChange file_change2(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::ADDED);
  FileChange file_change3(base::FilePath(FILE_PATH_LITERAL("a")),
                          FileChange::CHANGED);
  FileChange file_change4(base::FilePath(FILE_PATH_LITERAL("b")),
                          FileChange::ADDED);
  FileChange file_change5(base::FilePath(FILE_PATH_LITERAL("c")),
                          FileChange::DELETED);

  // Comparison operator should not return true for equal values.
  EXPECT_FALSE(file_change1 < file_change1);
  EXPECT_FALSE(file_change2 < file_change2);
  EXPECT_FALSE(file_change3 < file_change3);
  EXPECT_FALSE(file_change4 < file_change4);
  EXPECT_FALSE(file_change5 < file_change5);

  EXPECT_LT(file_change1, file_change2);
  EXPECT_LT(file_change1, file_change3);
  EXPECT_LT(file_change1, file_change4);
  EXPECT_LT(file_change1, file_change5);

  EXPECT_FALSE(file_change2 < file_change1);
  EXPECT_LT(file_change2, file_change3);
  EXPECT_LT(file_change2, file_change4);
  EXPECT_LT(file_change2, file_change5);

  EXPECT_FALSE(file_change3 < file_change1);
  EXPECT_FALSE(file_change3 < file_change2);
  EXPECT_LT(file_change3, file_change4);
  EXPECT_LT(file_change3, file_change5);

  EXPECT_FALSE(file_change4 < file_change1);
  EXPECT_FALSE(file_change4 < file_change2);
  EXPECT_FALSE(file_change4 < file_change3);
  EXPECT_LT(file_change4, file_change5);
}

}  // namespace drive
