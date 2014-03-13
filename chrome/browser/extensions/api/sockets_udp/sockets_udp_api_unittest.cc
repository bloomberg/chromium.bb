// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace extensions {
namespace core_api {

static KeyedService* ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));
  return ApiResourceManager<
      ResumableUDPSocket>::CreateApiResourceManagerForTest(context, id);
}

class SocketsUdpUnitTest : public ExtensionApiUnittest {
 public:
  virtual void SetUp() {
    ExtensionApiUnittest::SetUp();

    ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(browser()->profile(),
                                  ApiResourceManagerTestFactory);
  }
};

TEST_F(SocketsUdpUnitTest, Create) {
  // Get BrowserThread
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));

  // Create SocketCreateFunction and put it on BrowserThread
  SocketsUdpCreateFunction* function = new SocketsUdpCreateFunction();
  function->set_work_thread_id(id);

  // Run tests
  scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDictionary(
      function, "[{\"persistent\": true, \"name\": \"foo\"}]"));
  ASSERT_TRUE(result.get());
}

}  // namespace core_api
}  // namespace extensions
