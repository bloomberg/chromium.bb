// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/test_callback_receiver.h"

#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

TEST(TestCallbackReceiver, BasicClosure) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<> closure_receiver;

  auto closure = closure_receiver.callback();

  EXPECT_FALSE(closure_receiver.was_called());
  EXPECT_FALSE(closure_receiver.result().has_value());
  EXPECT_TRUE(closure);
  std::move(closure).Run();

  EXPECT_TRUE(closure_receiver.was_called());
  EXPECT_TRUE(closure_receiver.result().has_value());
  EXPECT_EQ(std::tuple<>(), closure_receiver.TakeResult());
  EXPECT_TRUE(closure_receiver.was_called());
  EXPECT_FALSE(closure_receiver.result().has_value());
}

TEST(TestCallbackReceiver, BasicCopyableArgument) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<int> callback_receiver;

  auto callback = callback_receiver.callback();

  EXPECT_FALSE(callback_receiver.was_called());
  EXPECT_FALSE(callback_receiver.result().has_value());
  EXPECT_TRUE(callback);
  std::move(callback).Run(42);

  EXPECT_TRUE(callback_receiver.was_called());
  EXPECT_TRUE(callback_receiver.result().has_value());
  EXPECT_EQ(std::make_tuple<int>(42), callback_receiver.TakeResult());
  EXPECT_TRUE(callback_receiver.was_called());
  EXPECT_FALSE(callback_receiver.result().has_value());
}

TEST(TestCallbackReceiver, MoveOnlyArgumentIsMoved) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<std::unique_ptr<int>> callback_receiver;

  auto callback = callback_receiver.callback();
  std::move(callback).Run(std::make_unique<int>(42));

  auto result = callback_receiver.TakeResult();
  static_assert(std::tuple_size<decltype(result)>::value == 1u,
                "The returned tuple should be one-dimensional");
  ASSERT_TRUE(std::get<0>(result));
  EXPECT_EQ(42, *std::get<0>(result));
}

TEST(TestCallbackReceiver, ReferenceArgumentIsCopied) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<int&> callback_receiver;

  int passed_in_value = 42;
  auto callback = callback_receiver.callback();

  EXPECT_FALSE(callback_receiver.result().has_value());

  std::move(callback).Run(passed_in_value);

  EXPECT_TRUE(callback_receiver.result().has_value());

  const int& received_value = std::get<0>(*callback_receiver.result());
  EXPECT_EQ(passed_in_value, received_value);

  passed_in_value = 43;
  EXPECT_NE(passed_in_value, received_value);

  callback_receiver.TakeResult();
  EXPECT_FALSE(callback_receiver.result().has_value());
}

TEST(TestCallbackReceiver, StatusAndValue) {
  enum class TestStatus { NOT_OK, OK };

  base::test::ScopedTaskEnvironment task_environment;
  StatusAndValueCallbackReceiver<TestStatus, std::unique_ptr<int>>
      callback_receiver;

  auto callback = callback_receiver.callback();

  EXPECT_FALSE(callback_receiver.was_called());
  EXPECT_TRUE(callback);
  std::move(callback).Run(TestStatus::OK, std::make_unique<int>(1337));

  EXPECT_TRUE(callback_receiver.was_called());
  EXPECT_EQ(TestStatus::OK, callback_receiver.status());
  EXPECT_EQ(1337, *callback_receiver.value());
}

TEST(TestCallbackReceiver, WaitForCallback) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<> closure_receiver;
  auto closure = closure_receiver.callback();

  EXPECT_FALSE(closure_receiver.was_called());
  EXPECT_TRUE(closure);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&closure]() { std::move(closure).Run(); }));

  EXPECT_FALSE(closure_receiver.was_called());
  closure_receiver.WaitForCallback();
  EXPECT_TRUE(closure_receiver.was_called());
}

TEST(TestCallbackReceiver, WaitForCallbackAfterCallback) {
  base::test::ScopedTaskEnvironment task_environment;
  TestCallbackReceiver<> closure_receiver;

  auto closure = closure_receiver.callback();

  EXPECT_FALSE(closure_receiver.was_called());
  EXPECT_TRUE(closure);

  std::move(closure).Run();

  EXPECT_TRUE(closure_receiver.was_called());
  closure_receiver.WaitForCallback();
  EXPECT_TRUE(closure_receiver.was_called());
}

}  // namespace test
}  // namespace device
