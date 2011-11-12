// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionAPI;

TEST(ExtensionAPI, IsPrivileged) {
  ExtensionAPI* extension_api = ExtensionAPI::GetInstance();
  EXPECT_FALSE(extension_api->IsPrivileged("extension.connect"));
  EXPECT_FALSE(extension_api->IsPrivileged("extension.onConnect"));

  // Properties are not supported yet.
  EXPECT_TRUE(extension_api->IsPrivileged("extension.lastError"));

  // Default unknown names to privileged for paranoia's sake.
  EXPECT_TRUE(extension_api->IsPrivileged(""));
  EXPECT_TRUE(extension_api->IsPrivileged("<unknown-namespace>"));
  EXPECT_TRUE(extension_api->IsPrivileged("extension.<unknown-member>"));

  // Exists, but privileged.
  EXPECT_TRUE(extension_api->IsPrivileged("extension.getViews"));
  EXPECT_TRUE(extension_api->IsPrivileged("history.search"));
}
