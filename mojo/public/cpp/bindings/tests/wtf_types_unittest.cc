// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/wtf_serialization.h"
#include "mojo/public/cpp/bindings/tests/variant_test_util.h"
#include "mojo/public/interfaces/bindings/tests/test_wtf_types.mojom-blink.h"
#include "mojo/public/interfaces/bindings/tests/test_wtf_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kHelloWorld[] = "hello world";

// Replace the "o"s in "hello world" with "o"s with acute.
const char kUTF8HelloWorld[] = "hell\xC3\xB3 w\xC3\xB3rld";

class TestWTFImpl : public TestWTF {
 public:
  explicit TestWTFImpl(TestWTFRequest request)
      : binding_(this, std::move(request)) {}

  // mojo::test::TestWTF implementation:
  void EchoString(const String& str,
                  const EchoStringCallback& callback) override {
    callback.Run(str);
  }

  void EchoStringArray(Array<String> arr,
                       const EchoStringArrayCallback& callback) override {
    callback.Run(std::move(arr));
  }

  void EchoStringMap(Map<String, String> str_map,
                     const EchoStringMapCallback& callback) override {
    callback.Run(std::move(str_map));
  }

 private:
  Binding<TestWTF> binding_;
};

class WTFTypesTest : public testing::Test {
 public:
  WTFTypesTest() {}

 private:
  base::MessageLoop loop_;
};

WTFArray<WTF::String> ConstructStringArray() {
  WTFArray<WTF::String> strs(4);
  // strs[0] is null.
  // strs[1] is empty.
  strs[1] = "";
  strs[2] = kHelloWorld;
  strs[3] = WTF::String::fromUTF8(kUTF8HelloWorld);

  return strs;
}

WTFMap<WTF::String, WTF::String> ConstructStringMap() {
  WTFMap<WTF::String, WTF::String> str_map;
  // A null string as value.
  str_map.insert("0", WTF::String());
  str_map.insert("1", kHelloWorld);
  str_map.insert("2", WTF::String::fromUTF8(kUTF8HelloWorld));

  return str_map;
}

void ExpectString(const WTF::String& expected_string,
                  const base::Closure& closure,
                  const WTF::String& string) {
  EXPECT_EQ(expected_string, string);
  closure.Run();
}

void ExpectStringArray(WTFArray<WTF::String> expected_arr,
                       const base::Closure& closure,
                       WTFArray<WTF::String> arr) {
  EXPECT_TRUE(expected_arr.Equals(arr));
  closure.Run();
}

void ExpectStringMap(WTFMap<WTF::String, WTF::String> expected_map,
                     const base::Closure& closure,
                     WTFMap<WTF::String, WTF::String> map) {
  EXPECT_TRUE(expected_map.Equals(map));
  closure.Run();
}

}  // namespace

TEST_F(WTFTypesTest, Serialization_WTFArrayToWTFArray) {
  WTFArray<WTF::String> strs = ConstructStringArray();
  WTFArray<WTF::String> cloned_strs = strs.Clone();

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<Array<mojo::String>>(
      cloned_strs, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<mojo::internal::String_Data*>* data;
  mojo::internal::ContainerValidateParams validate_params(
      0, true, new mojo::internal::ContainerValidateParams(0, false, nullptr));
  mojo::internal::Serialize<Array<mojo::String>>(cloned_strs, &buf, &data,
                                                 &validate_params, &context);

  WTFArray<WTF::String> strs2;
  mojo::internal::Deserialize<Array<mojo::String>>(data, &strs2, &context);

  EXPECT_TRUE(strs.Equals(strs2));
}

TEST_F(WTFTypesTest, Serialization_WTFVectorToWTFVector) {
  WTF::Vector<WTF::String> strs = ConstructStringArray().PassStorage();
  WTF::Vector<WTF::String> cloned_strs = strs;

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<Array<mojo::String>>(
      cloned_strs, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<mojo::internal::String_Data*>* data;
  mojo::internal::ContainerValidateParams validate_params(
      0, true, new mojo::internal::ContainerValidateParams(0, false, nullptr));
  mojo::internal::Serialize<Array<mojo::String>>(cloned_strs, &buf, &data,
                                                 &validate_params, &context);

  WTF::Vector<WTF::String> strs2;
  mojo::internal::Deserialize<Array<mojo::String>>(data, &strs2, &context);

  EXPECT_EQ(strs, strs2);
}

TEST_F(WTFTypesTest, Serialization_WTFArrayToMojoArray) {
  WTFArray<WTF::String> strs = ConstructStringArray();

  mojo::internal::SerializationContext context;
  size_t size =
      mojo::internal::PrepareToSerialize<Array<mojo::String>>(strs, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<mojo::internal::String_Data*>* data;
  mojo::internal::ContainerValidateParams validate_params(
      0, true, new mojo::internal::ContainerValidateParams(0, false, nullptr));
  mojo::internal::Serialize<Array<mojo::String>>(strs, &buf, &data,
                                                 &validate_params, &context);

  Array<mojo::String> strs2;
  mojo::internal::Deserialize<Array<mojo::String>>(data, &strs2, &context);

  ASSERT_EQ(4u, strs2.size());
  EXPECT_TRUE(strs2[0].is_null());
  EXPECT_TRUE("" == strs2[1]);
  EXPECT_TRUE(kHelloWorld == strs2[2]);
  EXPECT_TRUE(kUTF8HelloWorld == strs2[3]);
}

TEST_F(WTFTypesTest, Serialization_WTFMapToWTFMap) {
  using WTFType = WTFMap<WTF::String, WTF::String>;
  using MojomType = Map<mojo::String, mojo::String>;

  WTFType str_map = ConstructStringMap();
  WTFType cloned_str_map = str_map.Clone();

  mojo::internal::SerializationContext context;
  size_t size =
      mojo::internal::PrepareToSerialize<MojomType>(cloned_str_map, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(
          0, false,
          new mojo::internal::ContainerValidateParams(0, false, nullptr)),
      new mojo::internal::ContainerValidateParams(
          0, true,
          new mojo::internal::ContainerValidateParams(0, false, nullptr)));
  mojo::internal::Serialize<MojomType>(cloned_str_map, &buf, &data,
                                       &validate_params, &context);

  WTFType str_map2;
  mojo::internal::Deserialize<MojomType>(data, &str_map2, &context);

  EXPECT_TRUE(str_map.Equals(str_map2));
}

TEST_F(WTFTypesTest, Serialization_WTFMapToMojoMap) {
  using WTFType = WTFMap<WTF::String, WTF::String>;
  using MojomType = Map<mojo::String, mojo::String>;

  WTFType str_map = ConstructStringMap();

  mojo::internal::SerializationContext context;
  size_t size =
      mojo::internal::PrepareToSerialize<MojomType>(str_map, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(
          0, false,
          new mojo::internal::ContainerValidateParams(0, false, nullptr)),
      new mojo::internal::ContainerValidateParams(
          0, true,
          new mojo::internal::ContainerValidateParams(0, false, nullptr)));
  mojo::internal::Serialize<MojomType>(str_map, &buf, &data, &validate_params,
                                       &context);

  MojomType str_map2;
  mojo::internal::Deserialize<MojomType>(data, &str_map2, &context);

  ASSERT_EQ(3u, str_map2.size());
  EXPECT_TRUE(str_map2["0"].is_null());
  EXPECT_TRUE(kHelloWorld == str_map2["1"]);
  EXPECT_TRUE(kUTF8HelloWorld == str_map2["2"]);
}

TEST_F(WTFTypesTest, Serialization_PublicAPI) {
  blink::TestWTFStructPtr input(blink::TestWTFStruct::New());
  input->str = kHelloWorld;
  input->integer = 42;

  blink::TestWTFStructPtr cloned_input = input.Clone();

  WTFArray<uint8_t> data = blink::TestWTFStruct::Serialize(&input);

  blink::TestWTFStructPtr output;
  ASSERT_TRUE(blink::TestWTFStruct::Deserialize(std::move(data), &output));
  EXPECT_TRUE(cloned_input.Equals(output));
}

TEST_F(WTFTypesTest, SendString) {
  blink::TestWTFPtr ptr;
  TestWTFImpl impl(ConvertInterfaceRequest<TestWTF>(GetProxy(&ptr)));

  WTFArray<WTF::String> strs = ConstructStringArray();

  for (size_t i = 0; i < strs.size(); ++i) {
    base::RunLoop loop;
    // Test that a WTF::String is unchanged after the following conversion:
    //   - serialized;
    //   - deserialized as mojo::String;
    //   - serialized;
    //   - deserialized as WTF::String.
    ptr->EchoString(strs[i],
                    base::Bind(&ExpectString, strs[i], loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(WTFTypesTest, SendStringArray) {
  blink::TestWTFPtr ptr;
  TestWTFImpl impl(ConvertInterfaceRequest<TestWTF>(GetProxy(&ptr)));

  WTFArray<WTF::String> arrs[3];
  // arrs[0] is empty.
  // arrs[1] is null.
  arrs[1] = nullptr;
  arrs[2] = ConstructStringArray();

  for (size_t i = 0; i < arraysize(arrs); ++i) {
    WTFArray<WTF::String> expected_arr = arrs[i].Clone();
    base::RunLoop loop;
    // Test that a mojo::WTFArray<WTF::String> is unchanged after the following
    // conversion:
    //   - serialized;
    //   - deserialized as mojo::Array<mojo::String>;
    //   - serialized;
    //   - deserialized as mojo::WTFArray<WTF::String>.
    ptr->EchoStringArray(
        std::move(arrs[i]),
        base::Bind(&ExpectStringArray, base::Passed(&expected_arr),
                   loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(WTFTypesTest, SendStringMap) {
  blink::TestWTFPtr ptr;
  TestWTFImpl impl(ConvertInterfaceRequest<TestWTF>(GetProxy(&ptr)));

  WTFMap<WTF::String, WTF::String> maps[3];
  // maps[0] is empty.
  // maps[1] is null.
  maps[1] = nullptr;
  maps[2] = ConstructStringMap();

  for (size_t i = 0; i < arraysize(maps); ++i) {
    WTFMap<WTF::String, WTF::String> expected_map = maps[i].Clone();
    base::RunLoop loop;
    // Test that a mojo::WTFMap<WTF::String, WTF::String> is unchanged after the
    // following conversion:
    //   - serialized;
    //   - deserialized as mojo::Map<mojo::String, mojo::String>;
    //   - serialized;
    //   - deserialized as mojo::WTFMap<WTF::String, WTF::String>.
    ptr->EchoStringMap(
        std::move(maps[i]),
        base::Bind(&ExpectStringMap, base::Passed(&expected_map),
                   loop.QuitClosure()));
    loop.Run();
  }
}

}  // namespace test
}  // namespace mojo
