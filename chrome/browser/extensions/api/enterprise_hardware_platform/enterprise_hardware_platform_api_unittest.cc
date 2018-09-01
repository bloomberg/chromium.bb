// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_hardware_platform/enterprise_hardware_platform_api.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class EnterpriseHardwarePlatformAPITest
    : public ExtensionServiceTestWithInstall {
 public:
  EnterpriseHardwarePlatformAPITest() = default;
  ~EnterpriseHardwarePlatformAPITest() override = default;
  Browser* browser() { return browser_.get(); }

 private:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();
    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_TABBED;
    params.window = browser_window_.get();
    browser_ = std::make_unique<Browser>(params);
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseHardwarePlatformAPITest);
};

TEST_F(EnterpriseHardwarePlatformAPITest, GetHardwarePlatformInfo) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  scoped_refptr<EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction>
      function =
          new EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction();
  function->set_extension(extension.get());
  function->set_has_callback(true);

  std::string args;
  base::JSONWriter::Write(base::ListValue(), &args);

  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser()));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  ASSERT_EQ(result->DictSize(), 2u);

  const base::Value* val =
      result->FindKeyOfType("manufacturer", base::Value::Type::STRING);
  ASSERT_TRUE(val);
  const std::string& manufacturer = val->GetString();

  val = result->FindKeyOfType("model", base::Value::Type::STRING);
  ASSERT_TRUE(val);
  const std::string& model = val->GetString();

  EXPECT_FALSE(manufacturer.empty());
  EXPECT_FALSE(model.empty());
}

}  // namespace extensions
