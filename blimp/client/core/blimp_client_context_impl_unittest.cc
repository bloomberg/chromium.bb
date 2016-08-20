// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_client_context_impl.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

class BlimpClientContextImplTest : public testing::Test {
 public:
  BlimpClientContextImplTest() : io_thread_("BlimpTestIO") {}
  ~BlimpClientContextImplTest() override {}

  void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  void TearDown() override {
    io_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::Thread io_thread_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImplTest);
};

TEST_F(BlimpClientContextImplTest,
       CreatedBlimpContentsGetsHelpersAttachedAndHasTabControlFeature) {
  BlimpClientContextImpl blimp_client_context(io_thread_.task_runner(),
                                              io_thread_.task_runner());
  TestBlimpClientContextDelegate delegate;
  blimp_client_context.SetDelegate(&delegate);

  BlimpContents* attached_blimp_contents = nullptr;

  EXPECT_CALL(delegate, AttachBlimpContentsHelpers(testing::_))
      .WillOnce(testing::SaveArg<0>(&attached_blimp_contents))
      .RetiresOnSaturation();

  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_client_context.CreateBlimpContents();
  DCHECK(blimp_contents);
  DCHECK_EQ(blimp_contents.get(), attached_blimp_contents);
}

}  // namespace
}  // namespace client
}  // namespace blimp
