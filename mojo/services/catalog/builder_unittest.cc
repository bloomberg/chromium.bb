// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/builder.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/shell/public/cpp/names.h"
#include "testing/gtest/include/gtest/gtest.h"

class BuilderTest : public testing::Test {
 public:
  BuilderTest() {}
  ~BuilderTest() override {}

 protected:
  scoped_ptr<base::Value> ReadManifest(const std::string& manifest) {
    base::FilePath manifest_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &manifest_path);
    manifest_path = manifest_path.AppendASCII(
        "mojo/services/catalog/data/" + manifest);

    JSONFileValueDeserializer deserializer(manifest_path);
    int error = 0;
    std::string message;
    // TODO(beng): probably want to do more detailed error checking. This should
    //             be done when figuring out if to unblock connection
    //             completion.
    return deserializer.Deserialize(&error, &message);
  }

 private:
  void SetUp() override {}
  void TearDown() override {}

  DISALLOW_COPY_AND_ASSIGN(BuilderTest);
};

namespace catalog {

TEST_F(BuilderTest, Simple) {
  scoped_ptr<base::Value> value = ReadManifest("simple");
  base::DictionaryValue* dictionary = nullptr;
  CHECK(value->GetAsDictionary(&dictionary));
  Entry entry = BuildEntry(*dictionary);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ(mojo::GetNamePath(entry.name), entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
}

TEST_F(BuilderTest, Instance) {
  scoped_ptr<base::Value> value = ReadManifest("instance");
  base::DictionaryValue* dictionary = nullptr;
  CHECK(value->GetAsDictionary(&dictionary));
  Entry entry = BuildEntry(*dictionary);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ("bar", entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
}

TEST_F(BuilderTest, Capabilities) {
  scoped_ptr<base::Value> value = ReadManifest("capabilities");
  base::DictionaryValue* dictionary = nullptr;
  CHECK(value->GetAsDictionary(&dictionary));
  Entry entry = BuildEntry(*dictionary);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ("bar", entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
  CapabilityFilter filter;
  AllowedInterfaces interfaces;
  interfaces.insert("mojo::Bar");
  filter["mojo:bar"] = interfaces;
  EXPECT_EQ(filter, entry.capabilities);
}

TEST_F(BuilderTest, Malformed) {
  scoped_ptr<base::Value> value = ReadManifest("malformed");
  EXPECT_FALSE(value.get());
}


}  // namespace catalog
