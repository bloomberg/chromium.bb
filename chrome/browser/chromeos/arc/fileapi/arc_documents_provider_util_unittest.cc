// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrl) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL("home/calico.jpg"), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlEmptyPath) {
  std::string authority;
  std::string root_document_id;
  // Assign a non-empty arbitrary path to make sure an empty path is
  // set in ParseDocumentsProviderUrl().
  base::FilePath path(FILE_PATH_LITERAL("foobar"));

  // Should accept a path pointing to a root directory.
  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL(""), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlEmptyPathSlash) {
  std::string authority;
  std::string root_document_id;
  // Assign a non-empty arbitrary path to make sure an empty path is
  // set in ParseDocumentsProviderUrl().
  base::FilePath path(FILE_PATH_LITERAL("foobar"));

  // Should accept a path pointing to a root directory.
  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL(""), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlInvalidType) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  // Not storage::kFileSystemTypeArcDocumentsProvider.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcContent,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlInvalidPath) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  // root_document_id part is missing.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("root-missing"))),
      &authority, &root_document_id, &path));

  // Leading / is missing.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "special/arc-documents-provider/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));

  // Not under /special.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "/invalid/arc-documents-provider/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));

  // Not under /special/arc-documents-provider.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "/special/something-else/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlUtf8) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(
              "/special/arc-documents-provider/cats/root/home/みけねこ.jpg")),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL("home/みけねこ.jpg"), path.value());
}

TEST(ArcDocumentsProviderUtilTest, BuildDocumentUrl) {
  EXPECT_EQ("content://Cat%20Provider/document/C%2B%2B",
            BuildDocumentUrl("Cat Provider", "C++").spec());
}

}  // namespace

}  // namespace arc
