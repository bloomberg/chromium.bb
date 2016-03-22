// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/entry.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/shell/public/cpp/capabilities.h"
#include "mojo/shell/public/cpp/names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace catalog {

class EntryTest : public testing::Test {
 public:
  EntryTest() {}
  ~EntryTest() override {}

 protected:
  scoped_ptr<Entry> ReadEntry(const std::string& manifest,
                              scoped_ptr<base::Value>* out_value) {
    scoped_ptr<base::Value> value = ReadManifest(manifest);
    base::DictionaryValue* dictionary = nullptr;
    CHECK(value->GetAsDictionary(&dictionary));
    if (out_value)
      *out_value = std::move(value);
    return Entry::Deserialize(*dictionary);
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

  DISALLOW_COPY_AND_ASSIGN(EntryTest);
};

TEST_F(EntryTest, Simple) {
  scoped_ptr<Entry> entry = ReadEntry("simple", nullptr);
  EXPECT_EQ("mojo:foo", entry->name());
  EXPECT_EQ(mojo::GetNamePath(entry->name()), entry->qualifier());
  EXPECT_EQ("Foo", entry->display_name());
}

TEST_F(EntryTest, Instance) {
  scoped_ptr<Entry> entry = ReadEntry("instance", nullptr);
  EXPECT_EQ("mojo:foo", entry->name());
  EXPECT_EQ("bar", entry->qualifier());
  EXPECT_EQ("Foo", entry->display_name());
}

TEST_F(EntryTest, Capabilities) {
  scoped_ptr<Entry> entry = ReadEntry("capabilities", nullptr);

  EXPECT_EQ("mojo:foo", entry->name());
  EXPECT_EQ("bar", entry->qualifier());
  EXPECT_EQ("Foo", entry->display_name());
  mojo::CapabilitySpec spec;
  mojo::CapabilityRequest request;
  request.interfaces.insert("mojo::Bar");
  spec.required["mojo:bar"] = request;
  EXPECT_EQ(spec, entry->capabilities());
}

TEST_F(EntryTest, Serialization) {
  scoped_ptr<base::Value> value;
  scoped_ptr<Entry> entry = ReadEntry("serialization", &value);

  scoped_ptr<base::DictionaryValue> serialized(entry->Serialize());

  // We can't just compare values, since during deserialization some of the
  // lists get converted to std::sets, which are sorted, so Value::Equals will
  // fail.
  scoped_ptr<Entry> reconstituted = Entry::Deserialize(*serialized.get());
  EXPECT_EQ(*entry, *reconstituted);
}

TEST_F(EntryTest, Malformed) {
  scoped_ptr<base::Value> value = ReadManifest("malformed");
  EXPECT_FALSE(value.get());
}


}  // namespace catalog
