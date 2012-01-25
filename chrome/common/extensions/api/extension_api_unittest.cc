// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
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

TEST(ExtensionAPI, Depends) {
  // Fake extension with the "ttsEngine" permission but not the "tts"
  // permission; it must load TTS.
  DictionaryValue manifest;
  manifest.SetString("name", "test extension");
  manifest.SetString("version", "1.0");
  {
    scoped_ptr<ListValue> permissions(new ListValue());
    permissions->Append(Value::CreateStringValue("ttsEngine"));
    manifest.Set("permissions", permissions.release());
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::CreateWithId(
      FilePath(), Extension::LOAD, manifest, Extension::NO_FLAGS, "x", &error));
  CHECK(extension.get());
  CHECK(error.empty());

  ExtensionAPI::SchemaMap schemas;
  ExtensionAPI::GetInstance()->GetSchemasForExtension(*extension, &schemas);
  EXPECT_EQ(1u, schemas.count("tts"));
}
