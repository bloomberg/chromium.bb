// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_browser_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;

namespace vr_shell {

namespace {

class MockBrowserInterface : public VrBrowserInterface {
 public:
  MockBrowserInterface() {}
  ~MockBrowserInterface() override {}

  MOCK_METHOD1(ContentSurfaceChanged, void(jobject));
  MOCK_METHOD0(GvrDelegateReady, void());
  MOCK_METHOD1(UpdateGamepadData, void(device::GvrGamepadData));
  MOCK_METHOD1(AppButtonGesturePerformed, void(UiInterface::Direction));

  MOCK_METHOD0(AppButtonClicked, void());
  MOCK_METHOD0(ForceExitVr, void());
  MOCK_METHOD2(
      RunVRDisplayInfoCallback,
      void(const base::Callback<void(device::mojom::VRDisplayInfoPtr)>&,
           device::mojom::VRDisplayInfoPtr*));
  MOCK_METHOD1(OnContentPaused, void(bool));

  // Stub this as scoped pointers don't work as mock method parameters.
  void ProcessContentGesture(std::unique_ptr<blink::WebInputEvent>) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserInterface);
};

}  // namespace

class UiSceneManagerTest : public testing::Test {
 public:
  void SetUp() override {
    browser_ = base::MakeUnique<MockBrowserInterface>();
    scene_ = base::MakeUnique<UiScene>();
    // TODO(mthiesse): When we have UI to test for CCT, we'll need to modify
    // setup to allow us to test CCT mode.
    bool in_cct = false;
    bool in_web_vr = true;
    manager_ = base::MakeUnique<UiSceneManager>(browser_.get(), scene_.get(),
                                                in_cct, in_web_vr);
  }

 protected:
  std::unique_ptr<MockBrowserInterface> browser_;
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiSceneManager> manager_;
};

TEST_F(UiSceneManagerTest, ContentPausesOnAppButtonClick) {
  InSequence s;

  EXPECT_TRUE(scene_->GetWebVrRenderingEnabled());

  // Clicking app button once should pause content rendering.
  EXPECT_CALL(*browser_, OnContentPaused(true)).Times(1);
  manager_->OnAppButtonClicked();
  EXPECT_FALSE(scene_->GetWebVrRenderingEnabled());

  // Clicking it again should resume content rendering.
  EXPECT_CALL(*browser_, OnContentPaused(false)).Times(1);
  manager_->OnAppButtonClicked();
  EXPECT_TRUE(scene_->GetWebVrRenderingEnabled());
}

}  // namespace vr_shell
