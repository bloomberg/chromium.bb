// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_attribute_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

namespace {

const char kPrivetStorageJSON[] =
    "{"
    " \"name\": \"foo\","
    " \"type\": \"dir\","
    " \"entries\" : [  "
    " { \"name\": \"bar\", \"type\": \"dir\" },"
    " { \"name\": \"baz.txt\", \"type\": \"file\", \"size\": 12345 }"
    " ]}";

class PrivetFileSystemAttributeCacheTest : public ::testing::Test {
 public:
  PrivetFileSystemAttributeCacheTest() {}

  virtual ~PrivetFileSystemAttributeCacheTest() {}

  PrivetFileSystemAttributeCache cache_;
};

TEST_F(PrivetFileSystemAttributeCacheTest, AllAdded) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(kPrivetStorageJSON));
  const base::DictionaryValue* dictionary_json;
  json->GetAsDictionary(&dictionary_json);

  cache_.AddFileInfoFromJSON(
      base::FilePath(FILE_PATH_LITERAL("/test/path/foo")), dictionary_json);

  const base::File::Info* info_foo =
      cache_.GetFileInfo(base::FilePath(FILE_PATH_LITERAL("/test/path/foo")));
  EXPECT_TRUE(info_foo->is_directory);
  EXPECT_FALSE(info_foo->is_symbolic_link);

  const base::File::Info* info_bar = cache_.GetFileInfo(
      base::FilePath(FILE_PATH_LITERAL("/test/path/foo/bar")));
  EXPECT_TRUE(info_bar->is_directory);
  EXPECT_FALSE(info_bar->is_symbolic_link);

  const base::File::Info* info_baz = cache_.GetFileInfo(
      base::FilePath(FILE_PATH_LITERAL("/test/path/foo/baz.txt")));
  EXPECT_FALSE(info_baz->is_directory);
  EXPECT_FALSE(info_baz->is_symbolic_link);
  EXPECT_EQ(12345, info_baz->size);

  const base::File::Info* info_notfound = cache_.GetFileInfo(
      base::FilePath(FILE_PATH_LITERAL("/test/path/foo/notfound.txt")));
  EXPECT_EQ(NULL, info_notfound);
}

}  // namespace

}  // namespace local_discovery
