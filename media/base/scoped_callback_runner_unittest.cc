// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/scoped_callback_runner.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void SetBool(bool* var, bool val) {
  *var = val;
}

void SetBoolFromRawPtr(bool* var, bool* val) {
  *var = *val;
}

void SetIntegers(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

void SetIntegerFromUniquePtr(int* var, std::unique_ptr<int> val) {
  *var = *val;
}

void SetString(std::string* var, const std::string val) {
  *var = val;
}

}  // namespace

TEST(ScopedCallbackRunnerTest, Closure_Run) {
  bool a = false;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetBool, &a, true));
  std::move(cb).Run();
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, Closure_Destruction) {
  bool a = false;
  { auto cb = ScopedCallbackRunner(base::BindOnce(&SetBool, &a, true)); }
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, SetBool_Run) {
  bool a = false;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetBool, &a), true);
  std::move(cb).Run(true);
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, SetBoolFromRawPtr_Run) {
  bool a = false;
  bool* b = new bool(false);
  bool c = true;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetBoolFromRawPtr, &a),
                                 base::Owned(b));
  std::move(cb).Run(&c);
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, SetBoolFromRawPtr_Destruction) {
  bool a = false;
  bool* b = new bool(true);
  {
    auto cb = ScopedCallbackRunner(base::BindOnce(&SetBoolFromRawPtr, &a),
                                   base::Owned(b));
  }
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, SetBool_Destruction) {
  bool a = false;
  { auto cb = ScopedCallbackRunner(base::BindOnce(&SetBool, &a), true); }
  EXPECT_TRUE(a);
}

TEST(ScopedCallbackRunnerTest, SetIntegers_Run) {
  int a = 0;
  int b = 0;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetIntegers, &a, &b), 3, 4);
  std::move(cb).Run(1, 2);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);
}

TEST(ScopedCallbackRunnerTest, SetIntegers_Destruction) {
  int a = 0;
  int b = 0;
  {
    auto cb = ScopedCallbackRunner(base::BindOnce(&SetIntegers, &a, &b), 3, 4);
  }
  EXPECT_EQ(a, 3);
  EXPECT_EQ(b, 4);
}

TEST(ScopedCallbackRunnerTest, SetIntegerFromUniquePtr_Run) {
  int a = 0;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetIntegerFromUniquePtr, &a),
                                 base::MakeUnique<int>(1));
  std::move(cb).Run(base::MakeUnique<int>(2));
  EXPECT_EQ(a, 2);
}

TEST(ScopedCallbackRunnerTest, SetIntegerFromUniquePtr_Destruction) {
  int a = 0;
  {
    auto cb = ScopedCallbackRunner(base::BindOnce(&SetIntegerFromUniquePtr, &a),
                                   base::MakeUnique<int>(1));
  }
  EXPECT_EQ(a, 1);
}

TEST(ScopedCallbackRunnerTest, SetString_Run) {
  std::string a;
  auto cb = ScopedCallbackRunner(base::BindOnce(&SetString, &a), "hello");
  std::move(cb).Run("world");
  EXPECT_EQ(a, "world");
}

TEST(ScopedCallbackRunnerTest, SetString_Destruction) {
  std::string a;
  { auto cb = ScopedCallbackRunner(base::BindOnce(&SetString, &a), "hello"); }
  EXPECT_EQ(a, "hello");
}

}  // namespace media
