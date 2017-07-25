// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "mojo/common/process_id.mojom.h"
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

class TestFilePathImpl : public TestFilePath {
 public:
  explicit TestFilePathImpl(TestFilePathRequest request)
      : binding_(this, std::move(request)) {}

  // TestFilePath implementation:
  void BounceFilePath(const base::FilePath& in,
                      BounceFilePathCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<TestFilePath> binding_;
};

class TestUnguessableTokenImpl : public TestUnguessableToken {
 public:
  explicit TestUnguessableTokenImpl(TestUnguessableTokenRequest request)
      : binding_(this, std::move(request)) {}

  // TestUnguessableToken implementation:
  void BounceNonce(const base::UnguessableToken& in,
                   BounceNonceCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<TestUnguessableToken> binding_;
};

class TestTimeImpl : public TestTime {
 public:
  explicit TestTimeImpl(TestTimeRequest request)
      : binding_(this, std::move(request)) {}

  // TestTime implementation:
  void BounceTime(base::Time in, BounceTimeCallback callback) override {
    std::move(callback).Run(in);
  }

  void BounceTimeDelta(base::TimeDelta in,
                       BounceTimeDeltaCallback callback) override {
    std::move(callback).Run(in);
  }

  void BounceTimeTicks(base::TimeTicks in,
                       BounceTimeTicksCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<TestTime> binding_;
};

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

class TestString16Impl : public TestString16 {
 public:
  explicit TestString16Impl(TestString16Request request)
      : binding_(this, std::move(request)) {}

  // TestString16 implementation:
  void BounceString16(const base::string16& in,
                      BounceString16Callback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<TestString16> binding_;
};

class TestFileImpl : public TestFile {
 public:
  explicit TestFileImpl(TestFileRequest request)
      : binding_(this, std::move(request)) {}

  // TestFile implementation:
  void BounceFile(base::File in, BounceFileCallback callback) override {
    std::move(callback).Run(std::move(in));
  }

 private:
  mojo::Binding<TestFile> binding_;
};

class TestTextDirectionImpl : public TestTextDirection {
 public:
  explicit TestTextDirectionImpl(TestTextDirectionRequest request)
      : binding_(this, std::move(request)) {}

  // TestTextDirection:
  void BounceTextDirection(base::i18n::TextDirection in,
                           BounceTextDirectionCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<TestTextDirection> binding_;
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

TEST_F(CommonCustomTypesTest, FilePath) {
  base::RunLoop run_loop;

  TestFilePathPtr ptr;
  TestFilePathImpl impl(MakeRequest(&ptr));

  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));

  ptr->BounceFilePath(file, ExpectResponse(&file, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, UnguessableToken) {
  base::RunLoop run_loop;

  TestUnguessableTokenPtr ptr;
  TestUnguessableTokenImpl impl(MakeRequest(&ptr));

  base::UnguessableToken token = base::UnguessableToken::Create();

  ptr->BounceNonce(token, ExpectResponse(&token, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, ProcessId) {
  base::ProcessId pid = base::GetCurrentProcId();
  base::ProcessId out_pid = base::kNullProcessId;
  ASSERT_NE(pid, out_pid);
  EXPECT_TRUE(mojom::ProcessId::Deserialize(mojom::ProcessId::Serialize(&pid),
                                            &out_pid));
  EXPECT_EQ(pid, out_pid);
}

TEST_F(CommonCustomTypesTest, Time) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(MakeRequest(&ptr));

  base::Time t = base::Time::Now();

  ptr->BounceTime(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeDelta) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(MakeRequest(&ptr));

  base::TimeDelta t = base::TimeDelta::FromDays(123);

  ptr->BounceTimeDelta(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeTicks) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(MakeRequest(&ptr));

  base::TimeTicks t = base::TimeTicks::Now();

  ptr->BounceTimeTicks(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, Value) {
  TestValuePtr ptr;
  TestValueImpl impl(MakeRequest(&ptr));

  std::unique_ptr<base::Value> output;

  ASSERT_TRUE(ptr->BounceValue(nullptr, &output));
  EXPECT_FALSE(output);

  auto input = base::MakeUnique<base::Value>();
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::MakeUnique<base::Value>(123);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::MakeUnique<base::Value>(1.23);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::MakeUnique<base::Value>(false);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::MakeUnique<base::Value>("test string");
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  input = base::Value::CreateWithCopiedBuffer("mojo", 4);
  ASSERT_TRUE(ptr->BounceValue(input->CreateDeepCopy(), &output));
  EXPECT_EQ(*input, *output);

  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetBoolean("bool", false);
  dict->SetInteger("int", 2);
  dict->SetString("string", "some string");
  dict->SetBoolean("nested.bool", true);
  dict->SetInteger("nested.int", 9);
  dict->Set("some_binary", base::Value::CreateWithCopiedBuffer("mojo", 4));
  dict->Set("null_value", base::MakeUnique<base::Value>());
  dict->SetIntegerWithoutPathExpansion("non_nested.int", 10);
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

  auto list = base::MakeUnique<base::ListValue>();
  list->AppendString("string");
  list->AppendDouble(42.1);
  list->AppendBoolean(true);
  list->Append(base::Value::CreateWithCopiedBuffer("mojo", 4));
  list->Append(base::MakeUnique<base::Value>());
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

TEST_F(CommonCustomTypesTest, String16) {
  base::RunLoop run_loop;

  TestString16Ptr ptr;
  TestString16Impl impl(MakeRequest(&ptr));

  base::string16 str16 = base::ASCIIToUTF16("hello world");

  ptr->BounceString16(str16, ExpectResponse(&str16, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, EmptyString16) {
  base::RunLoop run_loop;

  TestString16Ptr ptr;
  TestString16Impl impl(MakeRequest(&ptr));

  base::string16 str16;

  ptr->BounceString16(str16, ExpectResponse(&str16, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, File) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  TestFilePtr ptr;
  TestFileImpl impl(MakeRequest(&ptr));

  base::File file(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  const base::StringPiece test_content =
      "A test string to be stored in a test file";
  file.WriteAtCurrentPos(
      test_content.data(),
      base::CheckedNumeric<int>(test_content.size()).ValueOrDie());

  base::File file_out;
  ASSERT_TRUE(ptr->BounceFile(std::move(file), &file_out));
  std::vector<char> content(test_content.size());
  ASSERT_TRUE(file_out.IsValid());
  ASSERT_EQ(static_cast<int>(test_content.size()),
            file_out.Read(
                0, content.data(),
                base::CheckedNumeric<int>(test_content.size()).ValueOrDie()));
  EXPECT_EQ(test_content,
            base::StringPiece(content.data(), test_content.size()));
}

TEST_F(CommonCustomTypesTest, InvalidFile) {
  TestFilePtr ptr;
  TestFileImpl impl(MakeRequest(&ptr));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Test that |file_out| is set to an invalid file.
  base::File file_out(
      temp_dir.GetPath().AppendASCII("test_file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);

  ASSERT_TRUE(ptr->BounceFile(base::File(), &file_out));
  EXPECT_FALSE(file_out.IsValid());
}

TEST_F(CommonCustomTypesTest, TextDirection) {
  base::i18n::TextDirection kTestDirections[] = {base::i18n::LEFT_TO_RIGHT,
                                                 base::i18n::RIGHT_TO_LEFT,
                                                 base::i18n::UNKNOWN_DIRECTION};

  TestTextDirectionPtr ptr;
  TestTextDirectionImpl impl(MakeRequest(&ptr));

  for (size_t i = 0; i < arraysize(kTestDirections); i++) {
    base::i18n::TextDirection direction_out;
    ASSERT_TRUE(ptr->BounceTextDirection(kTestDirections[i], &direction_out));
    EXPECT_EQ(kTestDirections[i], direction_out);
  }
}

}  // namespace test
}  // namespace common
}  // namespace mojo
