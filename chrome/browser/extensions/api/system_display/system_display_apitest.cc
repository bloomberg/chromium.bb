// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/system_display_api.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/system_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"

namespace utils = extension_function_test_utils;

namespace extensions {

using api::system_display::Bounds;
using api::system_display::DisplayUnitInfo;

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  MockDisplayInfoProvider() {}

  virtual bool QueryInfo() OVERRIDE {
    info_.clear();
    for (int i = 0; i < 4; i++) {
      linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo());
      unit->id = base::IntToString(i);
      unit->name = "DISPLAY NAME FOR " + unit->id;
      if (i == 1)
        unit->mirroring_source_id = "0";
      unit->is_primary = i == 0 ? true : false;
      unit->is_internal = i == 0 ? true : false;
      unit->is_enabled = true;
      unit->rotation = (90 * i) % 360;
      unit->dpi_x = 96.0;
      unit->dpi_y = 96.0;
      unit->bounds.left = 0;
      unit->bounds.top = 0;
      unit->bounds.width = 1280;
      unit->bounds.height = 720;
      if (i == 0) {
        unit->overscan.left = 20;
        unit->overscan.top = 40;
        unit->overscan.right = 60;
        unit->overscan.bottom = 80;
      }
      unit->work_area.left = 0;
      unit->work_area.top = 0;
      unit->work_area.width = 960;
      unit->work_area.height = 720;
      info_.push_back(unit);
    }
    return true;
  }

  virtual void SetInfo(
      const std::string& display_id,
      const api::system_display::DisplayProperties& params,
      const SetInfoCallback& callback) OVERRIDE {
    // Should get called only once per test case.
    EXPECT_FALSE(set_info_value_);
    set_info_value_ = params.ToValue();
    set_info_display_id_ = display_id;
    callback.Run(true, std::string());
  }

  scoped_ptr<base::DictionaryValue> GetSetInfoValue() {
    return set_info_value_.Pass();
  }

  std::string GetSetInfoDisplayId() const {
    return set_info_display_id_;
  }

 private:
  virtual ~MockDisplayInfoProvider() {}

  scoped_ptr<base::DictionaryValue> set_info_value_;
  std::string set_info_display_id_;

  DISALLOW_COPY_AND_ASSIGN(MockDisplayInfoProvider);
};

class SystemDisplayApiTest: public ExtensionApiTest {
 public:
  SystemDisplayApiTest() {}
  virtual ~SystemDisplayApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    provider_ = new MockDisplayInfoProvider();
    // The |provider| will be co-owned by the singleton instance.
    DisplayInfoProvider::InitializeForTesting(provider_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // Has to be released before the main thread is gone.
    provider_ = NULL;
    ExtensionApiTest::CleanUpOnMainThread();
  }

 protected:
  scoped_refptr<MockDisplayInfoProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(SystemDisplayApiTest);
};

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, GetDisplay) {
  ASSERT_TRUE(RunPlatformAppTest("system/display")) << message_;
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplay) {
  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction>
      set_info_function(new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);

  EXPECT_EQ("Function available only on ChromeOS.",
            utils::RunFunctionAndReturnError(set_info_function.get(),
                                             "[\"display_id\", {}]",
                                             browser()));

  scoped_ptr<base::DictionaryValue> set_info = provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayNotKioskEnabled) {
  scoped_ptr<base::DictionaryValue> test_extension_value(utils::ParseDictionary(
      "{\n"
      "  \"name\": \"Test\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"app\": {\n"
      "    \"background\": {\n"
      "      \"scripts\": [\"background.js\"]\n"
      "    }\n"
      "  }\n"
      "}"));
  scoped_refptr<Extension> test_extension(
      utils::CreateExtension(test_extension_value.get()));

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction>
      set_info_function(new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_extension(test_extension.get());
  set_info_function->set_has_callback(true);

  EXPECT_EQ("The extension needs to be kiosk enabled to use the function.",
            utils::RunFunctionAndReturnError(set_info_function.get(),
                                             "[\"display_id\", {}]",
                                             browser()));

  scoped_ptr<base::DictionaryValue> set_info = provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayKioskEnabled) {
  scoped_ptr<base::DictionaryValue> test_extension_value(utils::ParseDictionary(
      "{\n"
      "  \"name\": \"Test\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"app\": {\n"
      "    \"background\": {\n"
      "      \"scripts\": [\"background.js\"]\n"
      "    }\n"
      "  },\n"
      "  \"kiosk_enabled\": true\n"
      "}"));
  scoped_refptr<Extension> test_extension(
      utils::CreateExtension(test_extension_value.get()));

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction>
      set_info_function(new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);
  set_info_function->set_extension(test_extension.get());

  ASSERT_TRUE(utils::RunFunction(
      set_info_function.get(),
      "[\"display_id\", {\n"
      "  \"isPrimary\": true,\n"
      "  \"mirroringSourceId\": \"mirroringId\",\n"
      "  \"boundsOriginX\": 100,\n"
      "  \"boundsOriginY\": 200,\n"
      "  \"rotation\": 90,\n"
      "  \"overscan\": {\"left\": 1, \"top\": 2, \"right\": 3, \"bottom\": 4}\n"
      "}]",
      browser(),
      utils::NONE));

  scoped_ptr<base::DictionaryValue> set_info = provider_->GetSetInfoValue();
  ASSERT_TRUE(set_info);
  EXPECT_TRUE(utils::GetBoolean(set_info.get(), "isPrimary"));
  EXPECT_EQ("mirroringId",
            utils::GetString(set_info.get(), "mirroringSourceId"));
  EXPECT_EQ(100, utils::GetInteger(set_info.get(), "boundsOriginX"));
  EXPECT_EQ(200, utils::GetInteger(set_info.get(), "boundsOriginY"));
  EXPECT_EQ(90, utils::GetInteger(set_info.get(), "rotation"));
  base::DictionaryValue* overscan;
  ASSERT_TRUE(set_info->GetDictionary("overscan", &overscan));
  EXPECT_EQ(1, utils::GetInteger(overscan, "left"));
  EXPECT_EQ(2, utils::GetInteger(overscan, "top"));
  EXPECT_EQ(3, utils::GetInteger(overscan, "right"));
  EXPECT_EQ(4, utils::GetInteger(overscan, "bottom"));

  EXPECT_EQ("display_id", provider_->GetSetInfoDisplayId());
}
#endif  // defined(OS_CHROMEOS)

} // namespace extensions
