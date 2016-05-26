// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "mojo/common/common_custom_types.mojom.h"
#include "mojo/common/test_common_custom_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {
namespace {

class TestFilePathImpl : public TestFilePath {
 public:
  explicit TestFilePathImpl(TestFilePathRequest request)
      : binding_(this, std::move(request)) {}

  // TestFilePath implementation:
  void BounceFilePath(const base::FilePath& in,
                      const BounceFilePathCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestFilePath> binding_;
};

class TestTimeImpl : public TestTime {
 public:
  explicit TestTimeImpl(TestTimeRequest request)
      : binding_(this, std::move(request)) {}

  // TestTime implementation:
  void BounceTime(const base::Time& in,
                  const BounceTimeCallback& callback) override {
    callback.Run(in);
  }

  void BounceTimeDelta(const base::TimeDelta& in,
                  const BounceTimeDeltaCallback& callback) override {
    callback.Run(in);
  }

  void BounceTimeTicks(const base::TimeTicks& in,
                  const BounceTimeTicksCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestTime> binding_;
};

class TestValueImpl : public TestValue {
 public:
  explicit TestValueImpl(TestValueRequest request)
      : binding_(this, std::move(request)) {}

  // TestValue implementation:
  void BounceDictionaryValue(
      const base::DictionaryValue& in,
      const BounceDictionaryValueCallback& callback) override {
    callback.Run(in);
  }
  void BounceListValue(const base::ListValue& in,
                       const BounceListValueCallback& callback) override {
    callback.Run(in);
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

TEST_F(CommonCustomTypesTest, FilePath) {
  base::RunLoop run_loop;

  TestFilePathPtr ptr;
  TestFilePathImpl impl(GetProxy(&ptr));

  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));

  ptr->BounceFilePath(file, [&run_loop, &file](const base::FilePath& out) {
    EXPECT_EQ(file, out);
    run_loop.Quit();
  });

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, Time) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::Time t = base::Time::Now();

  ptr->BounceTime(t, [&run_loop, &t](const base::Time& out) {
    EXPECT_EQ(t, out);
    run_loop.Quit();
  });

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeDelta) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::TimeDelta t = base::TimeDelta::FromDays(123);

  ptr->BounceTimeDelta(t, [&run_loop, &t](const base::TimeDelta& out) {
    EXPECT_EQ(t, out);
    run_loop.Quit();
  });

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeTicks) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::TimeTicks t = base::TimeTicks::Now();

  ptr->BounceTimeTicks(t, [&run_loop, &t](const base::TimeTicks& out) {
    EXPECT_EQ(t, out);
    run_loop.Quit();
  });

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, Value) {
  TestValuePtr ptr;
  TestValueImpl impl(GetProxy(&ptr));

  base::DictionaryValue dict;
  dict.SetBoolean("bool", false);
  dict.SetInteger("int", 2);
  dict.SetString("string", "some string");
  dict.SetBoolean("nested.bool", true);
  dict.SetInteger("nested.int", 9);
  dict.Set(
      "some_binary",
      base::WrapUnique(base::BinaryValue::CreateWithCopiedBuffer("mojo", 4)));
  {
    std::unique_ptr<base::ListValue> dict_list(new base::ListValue());
    dict_list->AppendString("string");
    dict_list->AppendBoolean(true);
    dict.Set("list", std::move(dict_list));
  }
  {
    base::RunLoop run_loop;
    ptr->BounceDictionaryValue(
        dict, [&run_loop, &dict](const base::DictionaryValue& out) {
          EXPECT_TRUE(dict.Equals(&out));
          run_loop.Quit();
        });
    run_loop.Run();
  }

  base::ListValue list;
  list.AppendString("string");
  list.AppendDouble(42.1);
  list.AppendBoolean(true);
  list.Append(
      base::WrapUnique(base::BinaryValue::CreateWithCopiedBuffer("mojo", 4)));
  {
    std::unique_ptr<base::DictionaryValue> list_dict(
        new base::DictionaryValue());
    list_dict->SetString("string", "str");
    list.Append(std::move(list_dict));
  }
  {
    base::RunLoop run_loop;
    ptr->BounceListValue(list, [&run_loop, &list](const base::ListValue& out) {
      EXPECT_TRUE(list.Equals(&out));
      run_loop.Quit();
    });
    run_loop.Run();
  }
}

}  // namespace test
}  // namespace common
}  // namespace mojo
