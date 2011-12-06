// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using namespace extension_function_test_utils;

namespace {

class SocketApiTest : public InProcessBrowserTest {
 public:
  virtual void SetUpMyCommandLine() {
    DoCommandLineSetup();
  }

  // Exposed as static method so that SocketExtension can easily get to it.
  static void DoCommandLineSetup() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePlatformApps);
  }
};

}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketCreateGood) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<Extension> empty_extension(CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      socket_create_function, "[\"udp\"]", browser(), NONE));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  DictionaryValue *value = static_cast<DictionaryValue*>(result.get());
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  EXPECT_TRUE(socketId > 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketCreateBad) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<Extension> empty_extension(CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  // TODO(miket): this test currently passes only because of artificial code
  // that doesn't run in production. Fix this when we're able to.
  RunFunctionAndReturnError(socket_create_function, "[\"xxxx\"]", browser(),
                            NONE);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SocketExtension) {
  SocketApiTest::DoCommandLineSetup();
  ASSERT_TRUE(RunExtensionTest("socket/api")) << message_;
}
