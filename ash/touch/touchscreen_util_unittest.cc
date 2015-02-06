// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/display/display_info.h"
#include "ash/touch/touchscreen_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace ash {

class TouchscreenUtilTest : public testing::Test {
 public:
  TouchscreenUtilTest() {}
  ~TouchscreenUtilTest() override {}

  void SetUp() override {
    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    {
      DisplayInfo display(1, std::string(), false);
      DisplayMode mode(gfx::Size(1920, 1080), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
      gfx::Display::SetInternalDisplayId(1);
    }

    {
      DisplayInfo display(2, std::string(), false);
      DisplayMode mode(gfx::Size(800, 600), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
    }

    // Display without native mode. Must not be matched to any touch screen.
    {
      DisplayInfo display(3, std::string(), false);
      displays_.push_back(display);
    }

    {
      DisplayInfo display(4, std::string(), false);
      DisplayMode mode(gfx::Size(1024, 768), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
    }
  }

  void TearDown() override { displays_.clear(); }

 protected:
  std::vector<DisplayInfo> displays_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenUtilTest);
};

TEST_F(TouchscreenUtilTest, NoTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;
  AssociateTouchscreens(&displays_, devices);

  for (size_t i = 0; i < displays_.size(); ++i)
    EXPECT_EQ(ui::TouchscreenDevice::kInvalidId,
              displays_[i].touch_device_id());
}

TEST_F(TouchscreenUtilTest, OneToOneMapping) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(800, 600), 0));
  devices.push_back(ui::TouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1024, 768), 0));

  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[0].touch_device_id());
  EXPECT_EQ(1u, displays_[1].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[2].touch_device_id());
  EXPECT_EQ(2u, displays_[3].touch_device_id());
}

TEST_F(TouchscreenUtilTest, MapToCorrectDisplaySize) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(ui::TouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1024, 768), 0));

  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[0].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[1].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[2].touch_device_id());
  EXPECT_EQ(2u, displays_[3].touch_device_id());
}

TEST_F(TouchscreenUtilTest, MapWhenSizeDiffersByOne) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(801, 600), 0));
  devices.push_back(ui::TouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1023, 768), 0));

  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[0].touch_device_id());
  EXPECT_EQ(1u, displays_[1].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[2].touch_device_id());
  EXPECT_EQ(2u, displays_[3].touch_device_id());
}

TEST_F(TouchscreenUtilTest, MapWhenSizesDoNotMatch) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1022, 768), 0));
  devices.push_back(ui::TouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(802, 600), 0));

  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[0].touch_device_id());
  EXPECT_EQ(1u, displays_[1].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[2].touch_device_id());
  EXPECT_EQ(2u, displays_[3].touch_device_id());
}

TEST_F(TouchscreenUtilTest, MapInternalTouchscreen) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1920, 1080), 0));
  devices.push_back(ui::TouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(9999, 888), 0));

  AssociateTouchscreens(&displays_, devices);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(2u, displays_[0].touch_device_id());
  EXPECT_EQ(1u, displays_[1].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[2].touch_device_id());
  EXPECT_EQ(ui::TouchscreenDevice::kInvalidId, displays_[3].touch_device_id());
}

}  // namespace ash
