// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace extensions {

class SocketUnitTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();

    TestExtensionSystem* system = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(browser()->profile()));
    system->CreateSocketManager();

    extension_ = utils::CreateEmptyExtensionWithLocation(
        extensions::Manifest::UNPACKED);
  }

  base::Value* RunFunctionWithExtension(
      UIThreadExtensionFunction* function, const std::string& args) {
    scoped_refptr<UIThreadExtensionFunction> delete_function(function);
    function->set_extension(extension_.get());
    return utils::RunFunctionAndReturnSingleResult(function, args, browser());
  }

  base::DictionaryValue* RunFunctionAndReturnDict(
      UIThreadExtensionFunction* function, const std::string& args) {
    base::Value* result = RunFunctionWithExtension(function, args);
    return result ? utils::ToDictionary(result) : NULL;
  }

 protected:
  scoped_refptr<extensions::Extension> extension_;
};

TEST_F(SocketUnitTest, Create) {
  // TODO(miket): enable this test. This will require teaching
  // SocketCreateFunction to do its work on a thread other than IO. Getting
  // this CL landed was hard enough already, so we're going to save this work
  // for another day.
  if (false) {
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new SocketCreateFunction(), "[\"tcp\"]"));
    ASSERT_TRUE(result.get());
  }
}

}  // namespace extensions
