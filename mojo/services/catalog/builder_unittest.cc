// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/builder.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/shell/public/cpp/capabilities.h"
#include "mojo/shell/public/cpp/names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace catalog {

class BuilderTest : public testing::Test {
 public:
  BuilderTest() {}
  ~BuilderTest() override {}

 protected:
  scoped_ptr<base::Value> ReadEntry(const std::string& manifest, Entry* entry) {
    DCHECK(entry);
    scoped_ptr<base::Value> value = ReadManifest(manifest);
    base::DictionaryValue* dictionary = nullptr;
    CHECK(value->GetAsDictionary(&dictionary));
    *entry = BuildEntry(*dictionary);
    return value;
  }

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

TEST_F(BuilderTest, Simple) {
  Entry entry;
  ReadEntry("simple", &entry);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ(mojo::GetNamePath(entry.name), entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
}

TEST_F(BuilderTest, Instance) {
  Entry entry;
  ReadEntry("instance", &entry);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ("bar", entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
}

TEST_F(BuilderTest, Capabilities) {
  Entry entry;
  ReadEntry("capabilities", &entry);

  EXPECT_EQ("mojo:foo", entry.name);
  EXPECT_EQ("bar", entry.qualifier);
  EXPECT_EQ("Foo", entry.display_name);
  mojo::CapabilitySpec spec;
  mojo::CapabilityRequest request;
  request.interfaces.insert("mojo::Bar");
  spec.required["mojo:bar"] = request;
  EXPECT_EQ(spec, entry.capabilities);
}

TEST_F(BuilderTest, Serialization) {
  Entry entry;
  scoped_ptr<base::Value> value = ReadEntry("serialization", &entry);

  scoped_ptr<base::DictionaryValue> serialized(SerializeEntry(entry));

  // We can't just compare values, since during deserialization some of the
  // lists get converted to std::sets, which are sorted, so Value::Equals will
  // fail.
  Entry reconstituted = BuildEntry(*serialized.get());
  EXPECT_EQ(entry, reconstituted);
}

TEST_F(BuilderTest, Malformed) {
  scoped_ptr<base::Value> value = ReadManifest("malformed");
  EXPECT_FALSE(value.get());
}


}  // namespace catalog
