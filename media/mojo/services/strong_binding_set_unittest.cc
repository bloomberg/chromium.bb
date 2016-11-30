// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/strong_binding_set.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/tests/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

using TestService = service_manager::TestService;
using TestServicePtr = service_manager::TestServicePtr;

const int kInvalidBindingId = 100;

class TestServiceImpl : public TestService {
 public:
  TestServiceImpl() { instance_count++; }
  ~TestServiceImpl() override { instance_count--; }

  // TestService implementation.
  void Test(const std::string& test_string,
            const TestCallback& callback) override {
    DVLOG(1) << __func__ << ": " << test_string;
    callback.Run();
  }

  static int instance_count;
};

int TestServiceImpl::instance_count = 0;

}  // namespace

class StrongBindingSetTest : public testing::Test {
 public:
  StrongBindingSetTest()
      : bindings_(base::MakeUnique<StrongBindingSet<TestService>>()) {}
  ~StrongBindingSetTest() override {}

 protected:
  void AddBindings() {
    binding_id_1_ = bindings_->AddBinding(base::MakeUnique<TestServiceImpl>(),
                                          mojo::GetProxy(&test_service_ptr_1_));
    EXPECT_EQ(1, TestServiceImpl::instance_count);

    binding_id_2_ = bindings_->AddBinding(base::MakeUnique<TestServiceImpl>(),
                                          mojo::GetProxy(&test_service_ptr_2_));
    EXPECT_EQ(2, TestServiceImpl::instance_count);
  }

  TestServicePtr test_service_ptr_1_;
  TestServicePtr test_service_ptr_2_;
  StrongBindingSet<TestService>::BindingId binding_id_1_ = 0;
  StrongBindingSet<TestService>::BindingId binding_id_2_ = 0;
  std::unique_ptr<StrongBindingSet<TestService>> bindings_;

 private:
  base::TestMessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(StrongBindingSetTest);
};

TEST_F(StrongBindingSetTest, AddBinding_Destruct) {
  AddBindings();

  bindings_.reset();
  EXPECT_EQ(0, TestServiceImpl::instance_count);
}

TEST_F(StrongBindingSetTest, AddBinding_ConnectionError) {
  AddBindings();

  test_service_ptr_1_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, TestServiceImpl::instance_count);

  test_service_ptr_2_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, TestServiceImpl::instance_count);
}

TEST_F(StrongBindingSetTest, AddBinding_RemoveBinding) {
  AddBindings();

  EXPECT_TRUE(bindings_->RemoveBinding(binding_id_1_));
  EXPECT_EQ(1, TestServiceImpl::instance_count);

  EXPECT_FALSE(bindings_->RemoveBinding(kInvalidBindingId));
  EXPECT_EQ(1, TestServiceImpl::instance_count);

  EXPECT_TRUE(bindings_->RemoveBinding(binding_id_2_));
  EXPECT_EQ(0, TestServiceImpl::instance_count);
}

}  // namespace media
