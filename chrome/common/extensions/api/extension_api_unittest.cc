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

namespace extensions {
namespace {

TEST(ExtensionAPI, IsPrivileged) {
  ExtensionAPI extension_api;

  EXPECT_FALSE(extension_api.IsPrivileged("extension.connect"));
  EXPECT_FALSE(extension_api.IsPrivileged("extension.onConnect"));

  // Properties are not supported yet.
  EXPECT_TRUE(extension_api.IsPrivileged("extension.lastError"));

  // Default unknown names to privileged for paranoia's sake.
  EXPECT_TRUE(extension_api.IsPrivileged(""));
  EXPECT_TRUE(extension_api.IsPrivileged("<unknown-namespace>"));
  EXPECT_TRUE(extension_api.IsPrivileged("extension.<unknown-member>"));

  // Exists, but privileged.
  EXPECT_TRUE(extension_api.IsPrivileged("extension.getViews"));
  EXPECT_TRUE(extension_api.IsPrivileged("history.search"));

  // Whole APIs that are unprivileged.
  EXPECT_FALSE(extension_api.IsPrivileged("app.getDetails"));
  EXPECT_FALSE(extension_api.IsPrivileged("app.isInstalled"));
  EXPECT_FALSE(extension_api.IsPrivileged("storage.local"));
  EXPECT_FALSE(extension_api.IsPrivileged("storage.local.onChanged"));
  EXPECT_FALSE(extension_api.IsPrivileged("storage.local.set"));
  EXPECT_FALSE(extension_api.IsPrivileged("storage.local.MAX_ITEMS"));
  EXPECT_FALSE(extension_api.IsPrivileged("storage.set"));
}

TEST(ExtensionAPI, LazyGetSchema) {
  ExtensionAPI apis;

  EXPECT_EQ(NULL, apis.GetSchema(""));
  EXPECT_EQ(NULL, apis.GetSchema(""));
  EXPECT_EQ(NULL, apis.GetSchema("experimental"));
  EXPECT_EQ(NULL, apis.GetSchema("experimental"));
  EXPECT_EQ(NULL, apis.GetSchema("foo"));
  EXPECT_EQ(NULL, apis.GetSchema("foo"));

  EXPECT_TRUE(apis.GetSchema("experimental.dns"));
  EXPECT_TRUE(apis.GetSchema("experimental.dns"));
  EXPECT_TRUE(apis.GetSchema("experimental.infobars"));
  EXPECT_TRUE(apis.GetSchema("experimental.infobars"));
  EXPECT_TRUE(apis.GetSchema("extension"));
  EXPECT_TRUE(apis.GetSchema("extension"));
  EXPECT_TRUE(apis.GetSchema("omnibox"));
  EXPECT_TRUE(apis.GetSchema("omnibox"));
  EXPECT_TRUE(apis.GetSchema("storage"));
  EXPECT_TRUE(apis.GetSchema("storage"));
}

scoped_refptr<Extension> CreateExtensionWithPermissions(
    const std::set<std::string>& permissions) {
  DictionaryValue manifest;
  manifest.SetString("name", "extension");
  manifest.SetString("version", "1.0");
  manifest.SetInteger("manifest_version", 2);
  {
    scoped_ptr<ListValue> permissions_list(new ListValue());
    for (std::set<std::string>::const_iterator i = permissions.begin();
        i != permissions.end(); ++i) {
      permissions_list->Append(Value::CreateStringValue(*i));
    }
    manifest.Set("permissions", permissions_list.release());
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      FilePath(), Extension::LOAD, manifest, Extension::NO_FLAGS, &error));
  CHECK(extension.get());
  CHECK(error.empty());

  return extension;
}

scoped_refptr<Extension> CreateExtensionWithPermission(
    const std::string& permission) {
  std::set<std::string> permissions;
  permissions.insert(permission);
  return CreateExtensionWithPermissions(permissions);
}

TEST(ExtensionAPI, ExtensionWithUnprivilegedAPIs) {
  scoped_refptr<Extension> extension;
  {
    std::set<std::string> permissions;
    permissions.insert("storage");
    permissions.insert("history");
    extension = CreateExtensionWithPermissions(permissions);
  }

  ExtensionAPI extension_api;

  scoped_ptr<std::set<std::string> > privileged_apis =
      extension_api.GetAPIsForContext(
          Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());

  scoped_ptr<std::set<std::string> > unprivileged_apis =
      extension_api.GetAPIsForContext(
          Feature::UNBLESSED_EXTENSION_CONTEXT, extension.get(), GURL());

  scoped_ptr<std::set<std::string> > content_script_apis =
      extension_api.GetAPIsForContext(
          Feature::CONTENT_SCRIPT_CONTEXT, extension.get(), GURL());

  // "storage" is completely unprivileged.
  EXPECT_EQ(1u, privileged_apis->count("storage"));
  EXPECT_EQ(1u, unprivileged_apis->count("storage"));
  EXPECT_EQ(1u, content_script_apis->count("storage"));

  // "extension" is partially unprivileged.
  EXPECT_EQ(1u, privileged_apis->count("extension"));
  EXPECT_EQ(1u, unprivileged_apis->count("extension"));
  EXPECT_EQ(1u, content_script_apis->count("extension"));

  // "history" is entirely privileged.
  EXPECT_EQ(1u, privileged_apis->count("history"));
  EXPECT_EQ(0u, unprivileged_apis->count("history"));
  EXPECT_EQ(0u, content_script_apis->count("history"));
}

TEST(ExtensionAPI, ExtensionWithDependencies) {
  // Extension with the "ttsEngine" permission but not the "tts" permission; it
  // must load TTS.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("ttsEngine");
    scoped_ptr<std::set<std::string> > apis =
        ExtensionAPI().GetAPIsForContext(
            Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());
    EXPECT_EQ(1u, apis->count("ttsEngine"));
    EXPECT_EQ(1u, apis->count("tts"));
  }

  // Conversely, extension with the "tts" permission but not the "ttsEngine"
  // permission shouldn't get the "ttsEngine" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("tts");
    scoped_ptr<std::set<std::string> > apis =
        ExtensionAPI().GetAPIsForContext(
            Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());
    EXPECT_EQ(0u, apis->count("ttsEngine"));
    EXPECT_EQ(1u, apis->count("tts"));
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  scoped_ptr<std::set<std::string> > apis =
      api->GetAPIsForContext(Feature::WEB_PAGE_CONTEXT, NULL, GURL(url));
  return apis->count(api_name);
}

TEST(ExtensionAPI, URLMatching) {
  ExtensionAPI api;

  // "app" API is available to all URLs that content scripts can be injected.
  EXPECT_TRUE(MatchesURL(&api, "app", "http://example.com/example.html"));
  EXPECT_TRUE(MatchesURL(&api, "app", "https://blah.net"));
  EXPECT_TRUE(MatchesURL(&api, "app", "file://somefile.html"));

  // But not internal URLs (for chrome-extension:// the app API is injected by
  // GetSchemasForExtension).
  EXPECT_FALSE(MatchesURL(&api, "app", "about:flags"));
  EXPECT_FALSE(MatchesURL(&api, "app", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(&api, "app", "chrome-extension://fakeextension"));

  // "storage" API (for example) isn't available to any URLs.
  EXPECT_FALSE(MatchesURL(&api, "storage", "http://example.com/example.html"));
  EXPECT_FALSE(MatchesURL(&api, "storage", "https://blah.net"));
  EXPECT_FALSE(MatchesURL(&api, "storage", "file://somefile.html"));
  EXPECT_FALSE(MatchesURL(&api, "storage", "about:flags"));
  EXPECT_FALSE(MatchesURL(&api, "storage", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(&api, "storage", "chrome-extension://fakeextension"));
}

}  // namespace
}  // namespace extensions
