// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/context/blimp_client_context_impl.h"
#include "blimp/client/core/settings/settings.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "blimp/net/fake_blimp_message_processor.h"
#include "blimp/net/fake_pipe_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/android/window_android.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {
namespace {

class BlimpClientIntegrationTest : public testing::Test {
 public:
  BlimpClientIntegrationTest() : io_thread_("BlimpTestIO") {}
  ~BlimpClientIntegrationTest() override = default;

  void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    Settings::RegisterPrefs(prefs_.registry());

#if defined(OS_ANDROID)
    window_ = ui::WindowAndroid::CreateForTesting();
#endif  // defined(OS_ANDROID)

    std::unique_ptr<FakePipeManager> pipe_manager(
        base::MakeUnique<FakePipeManager>());
    pipe_manager_ = pipe_manager.get();

    context_ = base::MakeUnique<BlimpClientContextImpl>(
        io_thread_.task_runner(), io_thread_.task_runner(),
        base::MakeUnique<MockCompositorDependencies>(),
        base::MakeUnique<Settings>(&prefs_),
        std::move(pipe_manager));

    context_->SetDelegate(&delegate_);
  }

  void TearDown() override {
    io_thread_.Stop();
    base::RunLoop().RunUntilIdle();
#if defined(OS_ANDROID)
    window_->DestroyForTesting();
#endif  // defined(OS_ANDROID)
  }

 protected:
  base::Thread io_thread_;
  FakePipeManager* pipe_manager_;
  TestingPrefServiceSimple prefs_;
  gfx::NativeWindow window_ = nullptr;
  TestBlimpClientContextDelegate delegate_;
  std::unique_ptr<BlimpClientContextImpl> context_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientIntegrationTest);
};

TEST_F(BlimpClientIntegrationTest, Navigate) {
  std::unique_ptr<BlimpContents> blimp_contents =
      context_->CreateBlimpContents(window_);
  blimp_contents->GetNavigationController().GoForward();

  FakeBlimpMessageProcessor* nav_processor =
      pipe_manager_->GetOutgoingMessageProcessor(BlimpMessage::kNavigation);

  EXPECT_EQ(1U, nav_processor->messages().size());
  auto message = nav_processor->messages().at(0);
  EXPECT_EQ(NavigationMessage::GO_FORWARD, message.navigation().type());
}

}  // namespace
}  // namespace client
}  // namespace blimp
