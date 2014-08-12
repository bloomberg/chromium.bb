// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/browser/api/dns/mock_host_resolver_creator.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/test/shell_test.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace extensions {

class SocketsUdpApiTest : public AppShellTest {};

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpCreateGood) {
  scoped_refptr<extensions::core_api::SocketsUdpCreateFunction>
      socket_create_function(
          new extensions::core_api::SocketsUdpCreateFunction());
  scoped_refptr<Extension> empty_extension = test_util::CreateEmptyExtension();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_create_function.get(), "[]", browser_context()));

  base::DictionaryValue* value = NULL;
  ASSERT_TRUE(result->GetAsDictionary(&value));
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  ASSERT_TRUE(socketId > 0);
}

}  // namespace extensions
