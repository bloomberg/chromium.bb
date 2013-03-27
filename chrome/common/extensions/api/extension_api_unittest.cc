// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/api_feature.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "chrome/common/extensions/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

SimpleFeature* CreateAPIFeature() {
  return new APIFeature();
}

TEST(ExtensionAPI, Creation) {
  ExtensionAPI* shared_instance = ExtensionAPI::GetSharedInstance();
  EXPECT_EQ(shared_instance, ExtensionAPI::GetSharedInstance());

  scoped_ptr<ExtensionAPI> new_instance(
      ExtensionAPI::CreateWithDefaultConfiguration());
  EXPECT_NE(new_instance.get(),
            scoped_ptr<ExtensionAPI>(
                ExtensionAPI::CreateWithDefaultConfiguration()).get());

  ExtensionAPI empty_instance;

  struct {
    ExtensionAPI* api;
    bool expect_populated;
  } test_data[] = {
    { shared_instance, true },
    { new_instance.get(), true },
    { &empty_instance, false }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    EXPECT_EQ(test_data[i].expect_populated,
              test_data[i].api->GetSchema("bookmarks.create") != NULL);
  }
}

TEST(ExtensionAPI, SplitDependencyName) {
  struct {
    std::string input;
    std::string expected_feature_type;
    std::string expected_feature_name;
  } test_data[] = {
    { "", "api", "" },  // assumes "api" when no type is present
    { "foo", "api", "foo" },
    { "foo:", "foo", "" },
    { ":foo", "", "foo" },
    { "foo:bar", "foo", "bar" },
    { "foo:bar.baz", "foo", "bar.baz" }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    std::string feature_type;
    std::string feature_name;
    ExtensionAPI::SplitDependencyName(test_data[i].input, &feature_type,
                                      &feature_name);
    EXPECT_EQ(test_data[i].expected_feature_type, feature_type) << i;
    EXPECT_EQ(test_data[i].expected_feature_name, feature_name) << i;
  }
}

TEST(ExtensionAPI, IsPrivileged) {
  scoped_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  EXPECT_FALSE(extension_api->IsPrivileged("runtime.connect"));
  EXPECT_FALSE(extension_api->IsPrivileged("runtime.onConnect"));

  // Properties are not supported yet.
  EXPECT_TRUE(extension_api->IsPrivileged("runtime.lastError"));

  // Default unknown names to privileged for paranoia's sake.
  EXPECT_TRUE(extension_api->IsPrivileged(""));
  EXPECT_TRUE(extension_api->IsPrivileged("<unknown-namespace>"));
  EXPECT_TRUE(extension_api->IsPrivileged("extension.<unknown-member>"));

  // Exists, but privileged.
  EXPECT_TRUE(extension_api->IsPrivileged("extension.getViews"));
  EXPECT_TRUE(extension_api->IsPrivileged("history.search"));

  // Whole APIs that are unprivileged.
  EXPECT_FALSE(extension_api->IsPrivileged("app.getDetails"));
  EXPECT_FALSE(extension_api->IsPrivileged("app.isInstalled"));
  EXPECT_FALSE(extension_api->IsPrivileged("storage.local"));
  EXPECT_FALSE(extension_api->IsPrivileged("storage.local.onChanged"));
  EXPECT_FALSE(extension_api->IsPrivileged("storage.local.set"));
  EXPECT_FALSE(extension_api->IsPrivileged("storage.local.MAX_ITEMS"));
  EXPECT_FALSE(extension_api->IsPrivileged("storage.set"));
}

TEST(ExtensionAPI, IsPrivilegedFeatures) {
  struct {
    std::string api_full_name;
    bool expect_is_privilged;
  } test_data[] = {
    { "test1", false },
    { "test1.foo", true },
    { "test2", true },
    { "test2.foo", false },
    { "test2.bar", false },
    { "test2.baz", true },
    { "test3", false },
    { "test3.foo", true },
    { "test4", false }
  };

  base::FilePath api_features_path;
  PathService::Get(chrome::DIR_TEST_DATA, &api_features_path);
  api_features_path = api_features_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("privileged_api_features.json");

  std::string api_features_str;
  ASSERT_TRUE(file_util::ReadFileToString(
      api_features_path, &api_features_str)) << "privileged_api_features.json";

  base::DictionaryValue* value = static_cast<DictionaryValue*>(
      base::JSONReader::Read(api_features_str));
  BaseFeatureProvider api_feature_provider(*value, CreateAPIFeature);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    ExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    EXPECT_EQ(test_data[i].expect_is_privilged,
              api.IsPrivileged(test_data[i].api_full_name)) << i;
  }
}

TEST(ExtensionAPI, APIFeatures) {
  struct {
    std::string api_full_name;
    bool expect_is_available;
    Feature::Context context;
    GURL url;
  } test_data[] = {
    { "test1", false, Feature::WEB_PAGE_CONTEXT, GURL() },
    { "test1", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test2", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test2", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", false, Feature::WEB_PAGE_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test3", false, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test4", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test4.foo", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test4.foo", false, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "test4.foo.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test5", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test5.blah", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5.blah", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test6", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test6.foo", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test7", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.foo", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") }
  };

  base::FilePath api_features_path;
  PathService::Get(chrome::DIR_TEST_DATA, &api_features_path);
  api_features_path = api_features_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("api_features.json");

  std::string api_features_str;
  ASSERT_TRUE(file_util::ReadFileToString(
      api_features_path, &api_features_str)) << "api_features.json";

  base::DictionaryValue* value = static_cast<DictionaryValue*>(
      base::JSONReader::Read(api_features_str));
  BaseFeatureProvider api_feature_provider(*value, CreateAPIFeature);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    ExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (base::DictionaryValue::Iterator iter(*value); !iter.IsAtEnd();
         iter.Advance()) {
      if (iter.key().find(".") == std::string::npos)
        api.RegisterSchema(iter.key(), "");
    }

    EXPECT_EQ(test_data[i].expect_is_available,
              api.IsAvailable(test_data[i].api_full_name,
                              NULL,
                              test_data[i].context,
                              test_data[i].url).is_available()) << i;
  }
}

TEST(ExtensionAPI, LazyGetSchema) {
  scoped_ptr<ExtensionAPI> apis(ExtensionAPI::CreateWithDefaultConfiguration());

  EXPECT_EQ(NULL, apis->GetSchema(""));
  EXPECT_EQ(NULL, apis->GetSchema(""));
  EXPECT_EQ(NULL, apis->GetSchema("experimental"));
  EXPECT_EQ(NULL, apis->GetSchema("experimental"));
  EXPECT_EQ(NULL, apis->GetSchema("foo"));
  EXPECT_EQ(NULL, apis->GetSchema("foo"));

  EXPECT_TRUE(apis->GetSchema("experimental.dns"));
  EXPECT_TRUE(apis->GetSchema("experimental.dns"));
  EXPECT_TRUE(apis->GetSchema("experimental.infobars"));
  EXPECT_TRUE(apis->GetSchema("experimental.infobars"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("storage"));
  EXPECT_TRUE(apis->GetSchema("storage"));
}

scoped_refptr<Extension> CreateExtensionWithPermissions(
    const std::set<std::string>& permissions) {
  base::DictionaryValue manifest;
  manifest.SetString("name", "extension");
  manifest.SetString("version", "1.0");
  manifest.SetInteger("manifest_version", 2);
  {
    scoped_ptr<base::ListValue> permissions_list(new base::ListValue());
    for (std::set<std::string>::const_iterator i = permissions.begin();
        i != permissions.end(); ++i) {
      permissions_list->Append(new base::StringValue(*i));
    }
    manifest.Set("permissions", permissions_list.release());
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::UNPACKED,
      manifest, Extension::NO_FLAGS, &error));
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

  scoped_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // "storage" is completely unprivileged.
  EXPECT_TRUE(extension_api->IsAvailable("storage",
                                         extension.get(),
                                         Feature::BLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_TRUE(extension_api->IsAvailable("storage",
                                         extension.get(),
                                         Feature::UNBLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_TRUE(extension_api->IsAvailable("storage",
                                         extension.get(),
                                         Feature::CONTENT_SCRIPT_CONTEXT,
                                         GURL()).is_available());

  // "extension" is partially unprivileged.
  EXPECT_TRUE(extension_api->IsAvailable("extension",
                                         extension.get(),
                                         Feature::BLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_TRUE(extension_api->IsAvailable("extension",
                                         extension.get(),
                                         Feature::UNBLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_TRUE(extension_api->IsAvailable("extension",
                                         extension.get(),
                                         Feature::CONTENT_SCRIPT_CONTEXT,
                                         GURL()).is_available());

  // "history" is entirely privileged.
  EXPECT_TRUE(extension_api->IsAvailable("history",
                                         extension.get(),
                                         Feature::BLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("history",
                                          extension.get(),
                                          Feature::UNBLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("history",
                                          extension.get(),
                                          Feature::CONTENT_SCRIPT_CONTEXT,
                                          GURL()).is_available());
}

TEST(ExtensionAPI, ExtensionWithDependencies) {
  // Extension with the "ttsEngine" permission but not the "tts" permission; it
  // should not automatically get "tts" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("ttsEngine");
    scoped_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_TRUE(api->IsAvailable("ttsEngine",
                                 extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT,
                                 GURL()).is_available());
    EXPECT_FALSE(api->IsAvailable("tts",
                                  extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL()).is_available());
  }

  // Conversely, extension with the "tts" permission but not the "ttsEngine"
  // permission shouldn't get the "ttsEngine" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("tts");
    scoped_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_FALSE(api->IsAvailable("ttsEngine",
                                  extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL()).is_available());
    EXPECT_TRUE(api->IsAvailable("tts",
                                 extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT,
                                 GURL()).is_available());
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  return api->IsAvailable(
      api_name, NULL, Feature::WEB_PAGE_CONTEXT, GURL(url)).is_available();
}

TEST(ExtensionAPI, URLMatching) {
  scoped_ptr<ExtensionAPI> api(ExtensionAPI::CreateWithDefaultConfiguration());

  // "app" API is available to all URLs that content scripts can be injected.
  EXPECT_TRUE(MatchesURL(api.get(), "app", "http://example.com/example.html"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "https://blah.net"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "file://somefile.html"));

  // But not internal URLs (for chrome-extension:// the app API is injected by
  // GetSchemasForExtension).
  EXPECT_FALSE(MatchesURL(api.get(), "app", "about:flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "app", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "app",
                          "chrome-extension://fakeextension"));

  // "storage" API (for example) isn't available to any URLs.
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "http://example.com/example.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "https://blah.net"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "file://somefile.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "about:flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "chrome-extension://fakeextension"));
}

TEST(ExtensionAPI, GetAPINameFromFullName) {
  struct {
    std::string input;
    std::string api_name;
    std::string child_name;
  } test_data[] = {
    { "", "", "" },
    { "unknown", "", "" },
    { "bookmarks", "bookmarks", "" },
    { "bookmarks.", "bookmarks", "" },
    { ".bookmarks", "", "" },
    { "bookmarks.create", "bookmarks", "create" },
    { "bookmarks.create.", "bookmarks", "create." },
    { "bookmarks.create.monkey", "bookmarks", "create.monkey" },
    { "bookmarkManagerPrivate", "bookmarkManagerPrivate", "" },
    { "bookmarkManagerPrivate.copy", "bookmarkManagerPrivate", "copy" }
  };

  scoped_ptr<ExtensionAPI> api(ExtensionAPI::CreateWithDefaultConfiguration());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    std::string child_name;
    std::string api_name = api->GetAPINameFromFullName(test_data[i].input,
                                                       &child_name);
    EXPECT_EQ(test_data[i].api_name, api_name) << test_data[i].input;
    EXPECT_EQ(test_data[i].child_name, child_name) << test_data[i].input;
  }
}

TEST(ExtensionAPI, DefaultConfigurationFeatures) {
  scoped_ptr<ExtensionAPI> api(ExtensionAPI::CreateWithDefaultConfiguration());

  SimpleFeature* bookmarks = static_cast<SimpleFeature*>(
      api->GetFeatureDependency("api:bookmarks"));
  SimpleFeature* bookmarks_create = static_cast<SimpleFeature*>(
      api->GetFeatureDependency("api:bookmarks.create"));

  struct {
    SimpleFeature* feature;
    // TODO(aa): More stuff to test over time.
  } test_data[] = {
    { bookmarks },
    { bookmarks_create }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    SimpleFeature* feature = test_data[i].feature;
    ASSERT_TRUE(feature) << i;

    EXPECT_TRUE(feature->whitelist()->empty());
    EXPECT_TRUE(feature->extension_types()->empty());

    EXPECT_EQ(1u, feature->GetContexts()->size());
    EXPECT_TRUE(feature->GetContexts()->count(
        Feature::BLESSED_EXTENSION_CONTEXT));

    EXPECT_EQ(Feature::UNSPECIFIED_LOCATION, feature->location());
    EXPECT_EQ(Feature::UNSPECIFIED_PLATFORM, feature->platform());
    EXPECT_EQ(0, feature->min_manifest_version());
    EXPECT_EQ(0, feature->max_manifest_version());
  }
}

TEST(ExtensionAPI, FeaturesRequireContexts) {
  // TODO(cduvall): Make this check API featues.
  scoped_ptr<base::DictionaryValue> api_features1(new base::DictionaryValue());
  scoped_ptr<base::DictionaryValue> api_features2(new base::DictionaryValue());
  base::DictionaryValue* test1 = new base::DictionaryValue();
  base::DictionaryValue* test2 = new base::DictionaryValue();
  base::ListValue* contexts = new base::ListValue();
  contexts->Append(new base::StringValue("content_script"));
  test1->Set("contexts", contexts);
  api_features1->Set("test", test1);
  api_features2->Set("test", test2);

  struct {
    base::DictionaryValue* api_features;
    bool expect_success;
  } test_data[] = {
    { api_features1.get(), true },
    { api_features2.get(), false }
  };


  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    BaseFeatureProvider api_feature_provider(*test_data[i].api_features,
                                             CreateAPIFeature);
    Feature* feature = api_feature_provider.GetFeature("test");
    EXPECT_EQ(test_data[i].expect_success, feature != NULL) << i;
  }
}

static void GetDictionaryFromList(const base::DictionaryValue* schema,
                                  const std::string& list_name,
                                  const int list_index,
                                  const base::DictionaryValue** out) {
  const base::ListValue* list;
  EXPECT_TRUE(schema->GetList(list_name, &list));
  EXPECT_TRUE(list->GetDictionary(list_index, out));
}

TEST(ExtensionAPI, TypesHaveNamespace) {
  base::FilePath manifest_path;
  PathService::Get(chrome::DIR_TEST_DATA, &manifest_path);
  manifest_path = manifest_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("types_have_namespace.json");

  std::string manifest_str;
  ASSERT_TRUE(file_util::ReadFileToString(manifest_path, &manifest_str))
      << "Failed to load: " << manifest_path.value();

  ExtensionAPI api;
  api.RegisterSchema("test.foo", manifest_str);

  const base::DictionaryValue* schema = api.GetSchema("test.foo");

  const base::DictionaryValue* dict;
  const base::DictionaryValue* sub_dict;
  std::string type;

  GetDictionaryFromList(schema, "types", 0, &dict);
  EXPECT_TRUE(dict->GetString("id", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetString("customBindings", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetDictionary("properties", &sub_dict));
  const base::DictionaryValue* property;
  EXPECT_TRUE(sub_dict->GetDictionary("foo", &property));
  EXPECT_TRUE(property->GetString("$ref", &type));
  EXPECT_EQ("test.foo.OtherType", type);
  EXPECT_TRUE(sub_dict->GetDictionary("bar", &property));
  EXPECT_TRUE(property->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);

  GetDictionaryFromList(schema, "functions", 0, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetDictionary("returns", &sub_dict));
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);

  GetDictionaryFromList(schema, "functions", 1, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);
  EXPECT_TRUE(dict->GetDictionary("returns", &sub_dict));
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);

  GetDictionaryFromList(schema, "events", 0, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);
  GetDictionaryFromList(dict, "parameters", 1, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);
}

}  // namespace
}  // namespace extensions
