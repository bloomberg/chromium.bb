// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/begin_frame_args_test.h"
#include "content/browser/renderer_host/begin_frame_observer_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"

using testing::Mock;
using testing::_;

namespace content {
namespace {

class MockBeginFrameObserverProxyClient : public BeginFrameObserverProxyClient {
 public:
  MOCK_METHOD1(SendBeginFrame, void(const cc::BeginFrameArgs&));
};

class BeginFrameObserverProxyTest : public testing::Test {
 public:
  BeginFrameObserverProxyTest() {}
  ~BeginFrameObserverProxyTest() override {}

  void SetUp() override {
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    compositor_task_runner_ = new base::TestSimpleTaskRunner();
    compositor_.reset(
        new ui::Compositor(context_factory, compositor_task_runner_));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }

  void TearDown() override {
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
  }

  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_ptr<ui::Compositor> compositor_;
};

}  // namespace

TEST_F(BeginFrameObserverProxyTest, BeginFrameScheduling) {
  MockBeginFrameObserverProxyClient client;
  BeginFrameObserverProxy begin_frame_observer(&client);
  begin_frame_observer.SetCompositor(compositor());
  begin_frame_observer.SetNeedsBeginFrames(true);

  // SendBeginFrame is called when new |args| is delivered.
  cc::BeginFrameArgs args =
    cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                       base::TimeTicks::FromInternalValue(33));
  EXPECT_CALL(client, SendBeginFrame(args));
  compositor()->SendBeginFramesToChildren(args);
  Mock::VerifyAndClearExpectations(&client);

  // SendBeginFrame is called when new |args2| is delivered.
  cc::BeginFrameArgs args2 =
    cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                       base::TimeTicks::FromInternalValue(66));
  EXPECT_CALL(client, SendBeginFrame(args2));
  compositor()->SendBeginFramesToChildren(args2);
  Mock::VerifyAndClearExpectations(&client);

  // SendBeginFrame is not called when used |args2| is delivered.
  EXPECT_CALL(client, SendBeginFrame(_)).Times(0);
  compositor()->SendBeginFramesToChildren(args2);
  Mock::VerifyAndClearExpectations(&client);

  // SendBeginFrame is not called when compositor is reset.
  begin_frame_observer.ResetCompositor();
  cc::BeginFrameArgs args3 =
    cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                       base::TimeTicks::FromInternalValue(99));
  EXPECT_CALL(client, SendBeginFrame(_)).Times(0);
  compositor()->SendBeginFramesToChildren(args3);
  Mock::VerifyAndClearExpectations(&client);
}

}  // namespace content
