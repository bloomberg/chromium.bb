// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {

static std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return base::MakeUnique<ApiResourceManager<ResumableUDPSocket>>(context);
}

class SocketsUdpUnitTest : public ApiUnitTest {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();

    ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(browser_context(),
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
  std::unique_ptr<base::DictionaryValue> result(RunFunctionAndReturnDictionary(
      function, "[{\"persistent\": true, \"name\": \"foo\"}]"));
  ASSERT_TRUE(result.get());
}

}  // namespace api
}  // namespace extensions
