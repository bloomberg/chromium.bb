// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/support/compositor/mock_compositor_dependencies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/android/window_android.h"
#endif  // defined(OS_ANDROID)

using testing::_;

namespace {
const int kDummyTabId = 0;
}

namespace blimp {
namespace client {
namespace {

class BlimpContentsManagerTest : public testing::Test {
 public:
  BlimpContentsManagerTest() = default;

#if defined(OS_ANDROID)
  void SetUp() override { window_ = ui::WindowAndroid::CreateForTesting(); }

  void TearDown() override { window_->DestroyForTesting(); }
#endif  // defined(OS_ANDROID)

 protected:
  gfx::NativeWindow window_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsManagerTest);
};

class MockTabControlFeature : public TabControlFeature {
 public:
  MockTabControlFeature() {}
  ~MockTabControlFeature() override = default;

  MOCK_METHOD1(CreateTab, void(int));
  MOCK_METHOD1(CloseTab, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabControlFeature);
};

TEST_F(BlimpContentsManagerTest, GetExistingBlimpContents) {
  base::MessageLoop loop;
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  MockTabControlFeature tab_control_feature;

  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  BlimpContentsManager blimp_contents_manager(&compositor_deps, &ime_feature,
                                              nullptr, &render_widget_feature,
                                              &tab_control_feature);

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents(window_);
  int id = blimp_contents->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);
}

TEST_F(BlimpContentsManagerTest, GetNonExistingBlimpContents) {
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  MockTabControlFeature tab_control_feature;

  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  BlimpContentsManager blimp_contents_manager(&compositor_deps, &ime_feature,
                                              nullptr, &render_widget_feature,
                                              &tab_control_feature);

  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(kDummyTabId);
  EXPECT_EQ(nullptr, existing_contents);
}

TEST_F(BlimpContentsManagerTest, GetDestroyedBlimpContents) {
  base::MessageLoop loop;
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  MockTabControlFeature tab_control_feature;
  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  BlimpContentsManager blimp_contents_manager(&compositor_deps, &ime_feature,
                                              nullptr, &render_widget_feature,
                                              &tab_control_feature);
  int id;

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents(window_);
  id = blimp_contents.get()->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);

  EXPECT_CALL(tab_control_feature, CloseTab(id)).Times(1);
  blimp_contents.reset();

  loop.RunUntilIdle();
  EXPECT_EQ(nullptr, blimp_contents_manager.GetBlimpContents(id));
}

// TODO(mlliu): remove this test case (http://crbug.com/642558)
TEST_F(BlimpContentsManagerTest, CreateTwoBlimpContentsDestroyAndCreate) {
  base::MessageLoop loop;
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  MockTabControlFeature tab_control_feature;
  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  BlimpContentsManager blimp_contents_manager(&compositor_deps, &ime_feature,
                                              nullptr, &render_widget_feature,
                                              &tab_control_feature);

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(2);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents(window_);
  EXPECT_NE(blimp_contents, nullptr);

  std::unique_ptr<BlimpContentsImpl> second_blimp_contents =
      blimp_contents_manager.CreateBlimpContents(window_);
  EXPECT_EQ(second_blimp_contents, nullptr);

  blimp_contents.reset();
  std::unique_ptr<BlimpContentsImpl> third_blimp_contents =
      blimp_contents_manager.CreateBlimpContents(window_);
  EXPECT_NE(third_blimp_contents, nullptr);
}

}  // namespace
}  // namespace client
}  // namespace blimp
