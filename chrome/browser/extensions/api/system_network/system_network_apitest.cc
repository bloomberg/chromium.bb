// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/system_network/system_network_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using extensions::Extension;
using extensions::api::SystemNetworkGetNetworkInterfacesFunction;
using extensions::api::system_network::NetworkInterface;

namespace utils = extension_function_test_utils;

namespace {

class SystemNetworkApiTest : public ExtensionApiTest {
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, SystemNetworkExtension) {
  ASSERT_TRUE(RunExtensionTest("system/network")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, GetNetworkInterfaces) {
  scoped_refptr<SystemNetworkGetNetworkInterfacesFunction> socket_function(
      new SystemNetworkGetNetworkInterfacesFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      socket_function.get(), "[]", browser(), utils::NONE));
  ASSERT_EQ(base::Value::TYPE_LIST, result->GetType());

  // All we can confirm is that we have at least one address, but not what it
  // is.
  base::ListValue *value = static_cast<base::ListValue*>(result.get());
  ASSERT_TRUE(value->GetSize() > 0);

  for (base::ListValue::const_iterator it = value->begin();
      it != value->end(); ++it) {
    base::Value* network_interface_value = *it;

    NetworkInterface network_interface;
    ASSERT_TRUE(NetworkInterface::Populate(*network_interface_value,
                                           &network_interface));

    LOG(INFO) << "Network interface: address=" << network_interface.address
              << ", name=" << network_interface.name
              << ", prefix length=" << network_interface.prefix_length;
    ASSERT_NE(std::string(), network_interface.address);
    ASSERT_NE(std::string(), network_interface.name);
    ASSERT_LE(0, network_interface.prefix_length);
  }
}
