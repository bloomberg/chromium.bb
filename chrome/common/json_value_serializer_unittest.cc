// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class JSONFileValueSerializerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(JSONFileValueSerializerTest, Roundtrip) {
  base::FilePath original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  original_file_path =
      original_file_path.Append(FILE_PATH_LITERAL("serializer_test.js"));

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  scoped_ptr<Value> root;
  root.reset(deserializer.Deserialize(NULL, NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());

  Value* null_value = NULL;
  ASSERT_TRUE(root_dict->Get("null", &null_value));
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->IsType(Value::TYPE_NULL));

  bool bool_value = false;
  ASSERT_TRUE(root_dict->GetBoolean("bool", &bool_value));
  ASSERT_TRUE(bool_value);

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("int", &int_value));
  ASSERT_EQ(42, int_value);

  std::string string_value;
  ASSERT_TRUE(root_dict->GetString("string", &string_value));
  ASSERT_EQ("hello", string_value);

  // Now try writing.
  const base::FilePath written_file_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("test_output.js"));

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::TextContentsEqual(original_file_path,
                                           written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));
}

TEST_F(JSONFileValueSerializerTest, RoundtripNested) {
  base::FilePath original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  original_file_path =
      original_file_path.Append(FILE_PATH_LITERAL("serializer_nested_test.js"));

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  scoped_ptr<Value> root;
  root.reset(deserializer.Deserialize(NULL, NULL));
  ASSERT_TRUE(root.get());

  // Now try writing.
  base::FilePath written_file_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("test_output.js"));

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::TextContentsEqual(original_file_path,
                                           written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));
}

TEST_F(JSONFileValueSerializerTest, NoWhitespace) {
  base::FilePath source_file_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_file_path));
  source_file_path = source_file_path.Append(
      FILE_PATH_LITERAL("serializer_test_nowhitespace.js"));
  ASSERT_TRUE(file_util::PathExists(source_file_path));
  JSONFileValueSerializer serializer(source_file_path);
  scoped_ptr<Value> root;
  root.reset(serializer.Deserialize(NULL, NULL));
  ASSERT_TRUE(root.get());
}
