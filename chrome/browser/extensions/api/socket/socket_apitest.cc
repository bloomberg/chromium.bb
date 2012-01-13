// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/api/socket/test_echo_server_udp.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using namespace extension_function_test_utils;

namespace {

class SocketApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
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

// http://code.google.com/p/chromium/issues/detail?id=109337
//
// Currently believed to be fixed, but we're leaving it marked flaky
// for a few days to let it percolate through the trybots.
IN_PROC_BROWSER_TEST_F(SocketApiTest, FLAKY_SocketExtension) {
  scoped_refptr<extensions::TestEchoServerUDP> server =
      new extensions::TestEchoServerUDP();

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ASSERT_TRUE(StartTestServer());
  ExtensionTestMessageListener listener("port_please", true);

  int port = server->Start();
  ASSERT_TRUE(port > 0);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(port);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_TRUE(server->WaitUntilFinished());
}
