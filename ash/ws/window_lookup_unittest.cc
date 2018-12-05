// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_lookup.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

class WindowLookupTest : public AshTestBase {
 public:
  WindowLookupTest() = default;
  ~WindowLookupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    original_aura_env_mode_ =
        aura::test::EnvTestHelper().SetMode(aura::Env::Mode::MUS);
    feature_list_.InitWithFeatures({::features::kSingleProcessMash}, {});
    AshTestBase::SetUp();
  }
  void TearDown() override {
    AshTestBase::TearDown();
    aura::test::EnvTestHelper().SetMode(original_aura_env_mode_);
  }

 private:
  aura::Env::Mode original_aura_env_mode_ = aura::Env::Mode::LOCAL;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WindowLookupTest);
};

TEST_F(WindowLookupTest, AddWindowToTabletMode) {
  // TabletModeController calls to PowerManagerClient with a callback that is
  // run via a posted task. Run the loop now so that we know the task is
  // processed. Without this, the task gets processed later on, interfering
  // with this test.
  RunAllPendingInMessageLoop();

  // This test configures views with mus, which means it triggers some of the
  // DCHECKs ensuring Shell's Env is used.
  SetRunningOutsideAsh();

  // Configure views backed by mus.
  views::MusClient::InitParams mus_client_init_params;
  mus_client_init_params.connector =
      ash_test_helper()->GetWindowServiceConnector();
  mus_client_init_params.create_wm_state = false;
  mus_client_init_params.running_in_ws_process = true;
  views::MusClient mus_client(mus_client_init_params);

  // Create a widget. This widget is backed by mus.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.native_widget = mus_client.CreateNativeWidget(params, &widget);
  widget.Init(params);

  aura::Window* widget_root = widget.GetNativeWindow()->GetRootWindow();
  ASSERT_TRUE(CurrentContext());
  ASSERT_NE(widget_root, CurrentContext());

  // At this point ash hasn't created the proxy (request is async, over mojo).
  // So, there should be no proxy available yet.
  EXPECT_FALSE(window_lookup::IsProxyWindow(widget_root));
  EXPECT_FALSE(window_lookup::GetProxyWindowForClientWindow(widget_root));

  // Flush all messages from the WindowTreeClient to ensure the window service
  // has finished Widget creation.
  aura::test::WaitForAllChangesToComplete();

  // Now the proxy should be available.
  EXPECT_FALSE(window_lookup::IsProxyWindow(widget_root));
  aura::Window* proxy =
      window_lookup::GetProxyWindowForClientWindow(widget_root);
  ASSERT_TRUE(proxy);
  EXPECT_NE(proxy, widget_root);
  EXPECT_EQ(aura::Env::Mode::LOCAL, proxy->env()->mode());

  // Ensure we can go back to the client created window.
  EXPECT_EQ(widget_root, window_lookup::GetClientWindowForProxyWindow(proxy));
}

}  // namespace ash
