// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/system_display_api.h"

#include "base/debug/leak_annotations.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/system_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "ash/display/screen_ash.h"
#include "ash/shell.h"
#endif

namespace utils = extension_function_test_utils;

namespace extensions {

using api::system_display::Bounds;
using api::system_display::DisplayUnitInfo;
using gfx::Screen;

#if defined(OS_CHROMEOS)
class MockScreen : public ash::ScreenAsh {
 public:
  MockScreen() {
    for (int i = 0; i < 4; i++) {
      gfx::Rect bounds(0, 0, 1280, 720);
      gfx::Rect work_area(0, 0, 960, 720);
      gfx::Display display(i, bounds);
      display.set_work_area(work_area);
      displays_.push_back(display);
    }
  }
  virtual ~MockScreen() {}

 protected:
  // Overridden from gfx::Screen:
  virtual int GetNumDisplays() const OVERRIDE {
    return displays_.size();
  }
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE {
    return displays_;
  }
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE {
    return displays_[0];
  }
 private:
  std::vector<gfx::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(MockScreen);
};
#else
class MockScreen : public Screen {
 public:
  MockScreen() {
    for (int i = 0; i < 4; i++) {
      gfx::Rect bounds(0, 0, 1280, 720);
      gfx::Rect work_area(0, 0, 960, 720);
      gfx::Display display(i, bounds);
      display.set_work_area(work_area);
      displays_.push_back(display);
    }
  }
  virtual ~MockScreen() {}

 protected:
  // Overridden from gfx::Screen:
  virtual bool IsDIPEnabled() OVERRIDE { return true; }
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE  { return gfx::Point(); }
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE {
    return gfx::NativeWindow();
  }
  virtual gfx::NativeWindow GetWindowAtScreenPoint(
      const gfx::Point& point) OVERRIDE {
    return gfx::NativeWindow();
  }
  virtual int GetNumDisplays() const OVERRIDE {
    return displays_.size();
  }
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE {
    return displays_;
  }
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView window) const OVERRIDE {
    return gfx::Display(0);
  }
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE {
    return gfx::Display(0);
  }
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    return gfx::Display(0);
  }
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE {
    return displays_[0];
  }
  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE {}

 private:
  std::vector<gfx::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(MockScreen);
};
#endif

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  MockDisplayInfoProvider() {}

  virtual ~MockDisplayInfoProvider() {}

  virtual bool SetInfo(
      const std::string& display_id,
      const api::system_display::DisplayProperties& params,
      std::string* error) OVERRIDE {
    // Should get called only once per test case.
    EXPECT_FALSE(set_info_value_);
    set_info_value_ = params.ToValue();
    set_info_display_id_ = display_id;
    return true;
  }

  scoped_ptr<base::DictionaryValue> GetSetInfoValue() {
    return set_info_value_.Pass();
  }

  std::string GetSetInfoDisplayId() const {
    return set_info_display_id_;
  }

 private:
  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  virtual void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit) OVERRIDE {
    int64 id = display.id();
    unit->name = "DISPLAY NAME FOR " + base::Int64ToString(id);
    if (id == 1)
      unit->mirroring_source_id = "0";
    unit->is_primary = id == 0 ? true : false;
    unit->is_internal = id == 0 ? true : false;
    unit->is_enabled = true;
    unit->rotation = (90 * id) % 360;
    unit->dpi_x = 96.0;
    unit->dpi_y = 96.0;
    if (id == 0) {
      unit->overscan.left = 20;
      unit->overscan.top = 40;
      unit->overscan.right = 60;
      unit->overscan.bottom = 80;
    }
  }

  scoped_ptr<base::DictionaryValue> set_info_value_;
  std::string set_info_display_id_;

  DISALLOW_COPY_AND_ASSIGN(MockDisplayInfoProvider);
};

class SystemDisplayApiTest: public ExtensionApiTest {
 public:
  SystemDisplayApiTest() : provider_(new MockDisplayInfoProvider),
                           screen_(new MockScreen) {}

  virtual ~SystemDisplayApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    ANNOTATE_LEAKING_OBJECT_PTR(
        gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE));
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
#if defined(OS_CHROMEOS)
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   ash::Shell::GetScreen());
#endif
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  scoped_ptr<MockDisplayInfoProvider> provider_;
  scoped_ptr<gfx::Screen> screen_;

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
