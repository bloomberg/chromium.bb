// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/common/content_paths.h"
#include "net/base/test_data_directory.h"
#include "net/dns/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebSocket) {
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionTest("websocket")) << message_;
}
