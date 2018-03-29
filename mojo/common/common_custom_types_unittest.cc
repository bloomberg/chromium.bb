// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_math.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "mojo/common/test_common_custom_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {
namespace {

template <typename T>
struct BounceTestTraits {
  static void ExpectEquality(const T& a, const T& b) {
    EXPECT_EQ(a, b);
  }
};

template <typename T>
struct PassTraits {
  using Type = const T&;
};

template <>
struct PassTraits<base::Time> {
  using Type = base::Time;
};

template <>
struct PassTraits<base::TimeDelta> {
  using Type = base::TimeDelta;
};

template <>
struct PassTraits<base::TimeTicks> {
  using Type = base::TimeTicks;
};

template <typename T>
void DoExpectResponse(T* expected_value,
                      const base::Closure& closure,
                      typename PassTraits<T>::Type value) {
  BounceTestTraits<T>::ExpectEquality(*expected_value, value);
  closure.Run();
}

template <typename T>
base::Callback<void(typename PassTraits<T>::Type)> ExpectResponse(
    T* expected_value,
    const base::Closure& closure) {
  return base::Bind(&DoExpectResponse<T>, expected_value, closure);
}

class TestValueImpl : public TestValue {
 public:
  explicit TestValueImpl(TestValueRequest request)
      : binding_(this, std::move(request)) {}

  // TestValue implementation:
  void BounceDictionaryValue(std::unique_ptr<base::DictionaryValue> in,
                             BounceDictionaryValueCallback callback) override {
    std::move(callback).Run(std::move(in));
  }

  void BounceListValue(std::unique_ptr<base::ListValue> in,
                       BounceListValueCallback callback) override {
    std::move(callback).Run(std::move(in));
  }

  void BounceValue(std::unique_ptr<base::Value> in,
                   BounceValueCallback callback) override {
    std::move(callback).Run(std::move(in));
  }

 private:
  mojo::Binding<TestValue> binding_;
};

class CommonCustomTypesTest : public testing::Test {
 protected:
  CommonCustomTypesTest() {}
  ~CommonCustomTypesTest() override {}

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(CommonCustomTypesTest);
};

}  // namespace

TEST_F(CommonCustomTypesTest, Value) {
  TestValuePtr ptr;
  TestValueImpl impl(MakeRequest(&ptr));

  std::unique_ptr<base::Value> output;

  ASSERT_TRUE(ptr->BounceValue(nullptr, &output));
  EXPECT_FALSE(output);

  auto input = std::make_unique<base::Value>();
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = std::make_unique<base::Value>(123);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = std::make_unique<base::Value>(1.23);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = std::make_unique<base::Value>(false);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = std::make_unique<base::Value>("test string");
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::Value::CreateWithCopiedBuffer("mojo", 4);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetBoolean("bool", false);
  dict->SetInteger("int", 2);
  dict->SetString("string", "some string");
  dict->SetBoolean("nested.bool", true);
  dict->SetInteger("nested.int", 9);
  dict->Set("some_binary", base::Value::CreateWithCopiedBuffer("mojo", 4));
  dict->Set("null_value", std::make_unique<base::Value>());
  dict->SetKey("non_nested.int", base::Value(10));
  {
    std::unique_ptr<base::ListValue> dict_list(new base::ListValue());
    dict_list->AppendString("string");
    dict_list->AppendBoolean(true);
    dict->Set("list", std::move(dict_list));
  }

  std::unique_ptr<base::DictionaryValue> dict_output;
  ASSERT_TRUE(ptr->BounceDictionaryValue(dict->CreateDeepCopy(), &dict_output));
  EXPECT_EQ(*dict, *dict_output);

  input = std::move(dict);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  auto list = std::make_unique<base::ListValue>();
  list->AppendString("string");
  list->AppendDouble(42.1);
  list->AppendBoolean(true);
  list->Append(base::Value::CreateWithCopiedBuffer("mojo", 4));
  list->Append(std::make_unique<base::Value>());
  {
    std::unique_ptr<base::DictionaryValue> list_dict(
        new base::DictionaryValue());
    list_dict->SetString("string", "str");
    list->Append(std::move(list_dict));
  }
  std::unique_ptr<base::ListValue> list_output;
  ASSERT_TRUE(ptr->BounceListValue(list->CreateDeepCopy(), &list_output));
  EXPECT_EQ(*list, *list_output);

  input = std::move(list);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  ASSERT_EQ(*input, *output);
}

}  // namespace test
}  // namespace common
}  // namespace mojo
