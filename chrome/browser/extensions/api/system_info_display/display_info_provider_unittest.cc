// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DisplayInfoProvider unit tests.

#include "base/stl_util.h"
#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::system_info_display::DisplayUnitInfo;

struct TestDisplayBound {
  int left;
  int top;
  int width;
  int height;
};

struct TestDisplayInfo {
  std::string id;
  std::string name;
  bool is_primary;
  bool is_internal;
  bool is_enabled;
  double dpi_x;
  double dpi_y;
  TestDisplayBound bounds;
  TestDisplayBound work_area;
};

const struct TestDisplayInfo kTestingDisplayInfoData[] = {
  {"Display1", "Display Name1", true, true, true, 96.0, 96.0,
      {0, 0, 1280, 720}, {0, 0, 960, 720}},
  {"Display2", "Display Name2", true, true, true, 221.0, 221.0,
      {0, 0, 1280, 720}, {0, 0, 960, 720}}
};

class TestDisplayInfoProvider : public DisplayInfoProvider {
 public:
  virtual bool QueryInfo(DisplayInfo* info) OVERRIDE;

 private:
  virtual ~TestDisplayInfoProvider();
};

TestDisplayInfoProvider::~TestDisplayInfoProvider() {}

bool TestDisplayInfoProvider::QueryInfo(DisplayInfo* info) {
  if (info == NULL)
    return false;
  info->clear();
  for (size_t i = 0; i < arraysize(kTestingDisplayInfoData); ++i) {
    linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo());
    unit->id = kTestingDisplayInfoData[i].id;
    unit->name = kTestingDisplayInfoData[i].name;
    unit->is_enabled = kTestingDisplayInfoData[i].is_enabled;
    unit->is_primary = kTestingDisplayInfoData[i].is_primary;
    unit->is_internal = kTestingDisplayInfoData[i].is_internal;
    unit->dpi_x = kTestingDisplayInfoData[i].dpi_x;
    unit->dpi_y = kTestingDisplayInfoData[i].dpi_y;
    unit->bounds.left = kTestingDisplayInfoData[i].bounds.left;
    unit->bounds.top = kTestingDisplayInfoData[i].bounds.top;
    unit->bounds.width = kTestingDisplayInfoData[i].bounds.width;
    unit->bounds.height = kTestingDisplayInfoData[i].bounds.height;
    unit->work_area.left = kTestingDisplayInfoData[i].work_area.left;
    unit->work_area.top = kTestingDisplayInfoData[i].work_area.top;
    unit->work_area.width = kTestingDisplayInfoData[i].work_area.width;
    unit->work_area.height = kTestingDisplayInfoData[i].work_area.height;
    info->push_back(unit);
  }
  return true;
}

class DisplayInfoProviderTest : public testing::Test {
 public:
  DisplayInfoProviderTest();

 protected:
  scoped_refptr<TestDisplayInfoProvider> display_info_provider_;
};

DisplayInfoProviderTest::DisplayInfoProviderTest()
    : display_info_provider_(new TestDisplayInfoProvider()) {
}

TEST_F(DisplayInfoProviderTest, QueryDisplayInfo) {
  scoped_ptr<DisplayInfo> display_info(new DisplayInfo());
  EXPECT_TRUE(display_info_provider_->QueryInfo(display_info.get()));
  EXPECT_EQ(arraysize(kTestingDisplayInfoData), display_info->size());
  for (size_t i = 0; i < arraysize(kTestingDisplayInfoData); ++i) {
    const linked_ptr<api::system_info_display::DisplayUnitInfo> unit =
        (*display_info.get())[i];
    EXPECT_EQ(kTestingDisplayInfoData[i].id, unit->id);
    EXPECT_EQ(kTestingDisplayInfoData[i].name, unit->name);
    EXPECT_DOUBLE_EQ(kTestingDisplayInfoData[i].dpi_x, unit->dpi_x);
    EXPECT_DOUBLE_EQ(kTestingDisplayInfoData[i].dpi_y, unit->dpi_y);
    EXPECT_EQ(kTestingDisplayInfoData[i].is_enabled, unit->is_enabled);
    EXPECT_EQ(kTestingDisplayInfoData[i].is_internal, unit->is_internal);
    EXPECT_EQ(kTestingDisplayInfoData[i].is_primary, unit->is_primary);
    EXPECT_EQ(kTestingDisplayInfoData[i].bounds.left, unit->bounds.left);
    EXPECT_EQ(kTestingDisplayInfoData[i].bounds.top, unit->bounds.top);
    EXPECT_EQ(kTestingDisplayInfoData[i].bounds.width, unit->bounds.width);
    EXPECT_EQ(kTestingDisplayInfoData[i].bounds.height, unit->bounds.height);
    EXPECT_EQ(kTestingDisplayInfoData[i].work_area.left, unit->work_area.left);
    EXPECT_EQ(kTestingDisplayInfoData[i].work_area.top, unit->work_area.top);
    EXPECT_EQ(kTestingDisplayInfoData[i].work_area.width,
              unit->work_area.width);
    EXPECT_EQ(kTestingDisplayInfoData[i].work_area.height,
              unit->work_area.height);
  }
}

} // namespace extensions
