// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

KeyedService* ApiResourceManagerTestFactory(content::BrowserContext* context) {
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));
  return ApiResourceManager<Socket>::CreateApiResourceManagerForTest(context,
                                                                     id);
}

class SocketUnitTest : public ExtensionApiUnittest {
 public:
  virtual void SetUp() {
    ExtensionApiUnittest::SetUp();

    ApiResourceManager<Socket>::GetFactoryInstance()->SetTestingFactoryAndUse(
        browser()->profile(), ApiResourceManagerTestFactory);
  }
};

TEST_F(SocketUnitTest, Create) {
  // Get BrowserThread
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));

  // Create SocketCreateFunction and put it on BrowserThread
  SocketCreateFunction* function = new SocketCreateFunction();
  function->set_work_thread_id(id);

  // Run tests
  scoped_ptr<base::DictionaryValue> result(
      RunFunctionAndReturnDictionary(function, "[\"tcp\"]"));
  ASSERT_TRUE(result.get());
}

}  // namespace extensions
