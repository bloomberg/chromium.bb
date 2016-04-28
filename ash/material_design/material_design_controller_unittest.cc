// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/ash_switches.h"
#include "ash/material_design/material_design_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/material_design_controller_test_api.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Test fixture for the MaterialDesignController class.
class MaterialDesignControllerTest : public ash::test::AshTestBase {
 public:
  MaterialDesignControllerTest() = default;
  ~MaterialDesignControllerTest() override = default;

 protected:
  // ash::test::AshTestBase:
  void SetCommandLineValue(const std::string& value_string);

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTest);
};

void MaterialDesignControllerTest::SetCommandLineValue(
    const std::string& value_string) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kAshMaterialDesign, value_string);
}

class MaterialDesignControllerTestNonMaterial
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestNonMaterial() : MaterialDesignControllerTest() {}
  void SetUp() override {
    SetCommandLineValue("disabled");
    MaterialDesignControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestNonMaterial);
};

class MaterialDesignControllerTestMaterial
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestMaterial() : MaterialDesignControllerTest() {}
  void SetUp() override {
    SetCommandLineValue("enabled");
    MaterialDesignControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestMaterial);
};

class MaterialDesignControllerTestExperimental
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestExperimental() : MaterialDesignControllerTest() {}
  void SetUp() override {
    SetCommandLineValue("experimental");
    MaterialDesignControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestExperimental);
};

class MaterialDesignControllerTestInvalid
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestInvalid() : MaterialDesignControllerTest() {}
  void SetUp() override {
    SetCommandLineValue("1nvalid-valu3");
    MaterialDesignControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestInvalid);
};

}  // namespace

// Verify the current mode is reported as the default mode when no command line
// flag is added.
TEST_F(MaterialDesignControllerTest, NoCommandLineValueMapsToDefaultMode) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshMaterialDesign));
  EXPECT_EQ(test::MaterialDesignControllerTestAPI::DefaultMode() ==
                    MaterialDesignController::Mode::MATERIAL_NORMAL ||
                test::MaterialDesignControllerTestAPI::DefaultMode() ==
                    MaterialDesignController::Mode::MATERIAL_EXPERIMENTAL,
            MaterialDesignController::IsMaterial());
}

// Verify that MaterialDesignController::IsMaterial() will be false when
// initialized with command line flag "disabled".
TEST_F(MaterialDesignControllerTestNonMaterial, CommandLineValue) {
  EXPECT_FALSE(MaterialDesignController::IsMaterial());
  EXPECT_FALSE(MaterialDesignController::IsMaterialExperimental());
}

// Verify that MaterialDesignController::IsMaterial() will be true when
// initialized with command line flag "enabled".
TEST_F(MaterialDesignControllerTestMaterial, CommandLineValue) {
  EXPECT_TRUE(MaterialDesignController::IsMaterial());
  EXPECT_FALSE(MaterialDesignController::IsMaterialExperimental());
}

// Verify that MaterialDesignController::IsMaterialexperimental() will be true
// when initialized with command line flag "experimental".
TEST_F(MaterialDesignControllerTestExperimental, CommandLineValue) {
  EXPECT_TRUE(MaterialDesignController::IsMaterial());
  EXPECT_TRUE(MaterialDesignController::IsMaterialExperimental());
}

// Verify an invalid command line value uses the default mode.
TEST_F(MaterialDesignControllerTestInvalid, CommandLineValue) {
  EXPECT_EQ(test::MaterialDesignControllerTestAPI::DefaultMode() ==
                    MaterialDesignController::Mode::MATERIAL_NORMAL ||
                test::MaterialDesignControllerTestAPI::DefaultMode() ==
                    MaterialDesignController::Mode::MATERIAL_EXPERIMENTAL,
            MaterialDesignController::IsMaterial());
}

}  // namespace ash
