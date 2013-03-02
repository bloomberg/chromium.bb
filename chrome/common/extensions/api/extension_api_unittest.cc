// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "chrome/common/extensions/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class TestFeatureProvider : public FeatureProvider {
 public:
  explicit TestFeatureProvider(Feature::Context context)
      : context_(context) {
  }

  virtual Feature* GetFeature(const std::string& name) OVERRIDE {
    SimpleFeature* result = new SimpleFeature();
    result->set_name(name);
    result->extension_types()->insert(Manifest::TYPE_EXTENSION);
    result->GetContexts()->insert(context_);
    to_destroy_.push_back(make_linked_ptr(result));
    return result;
  }

 private:
  std::vector<linked_ptr<Feature> > to_destroy_;
  Feature::Context context_;
};

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
    std::string filename;
    std::string api_full_name;
    bool expect_is_privilged;
    Feature::Context test2_contexts;
  } test_data[] = {
    { "is_privileged_features_1.json", "test", false,
      Feature::UNSPECIFIED_CONTEXT },
    { "is_privileged_features_2.json", "test", true,
      Feature::UNSPECIFIED_CONTEXT },
    { "is_privileged_features_3.json", "test", false,
      Feature::UNSPECIFIED_CONTEXT },
    { "is_privileged_features_4.json", "test.bar", false,
      Feature::UNSPECIFIED_CONTEXT },
    { "is_privileged_features_5.json", "test.bar", true,
      Feature::BLESSED_EXTENSION_CONTEXT },
    { "is_privileged_features_5.json", "test.bar", false,
      Feature::UNBLESSED_EXTENSION_CONTEXT }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    base::FilePath manifest_path;
    PathService::Get(chrome::DIR_TEST_DATA, &manifest_path);
    manifest_path = manifest_path.AppendASCII("extensions")
        .AppendASCII("extension_api_unittest")
        .AppendASCII(test_data[i].filename);

    std::string manifest_str;
    ASSERT_TRUE(file_util::ReadFileToString(manifest_path, &manifest_str))
        << test_data[i].filename;

    ExtensionAPI api;
    api.RegisterSchema("test", manifest_str);

    TestFeatureProvider test2_provider(test_data[i].test2_contexts);
    if (test_data[i].test2_contexts != Feature::UNSPECIFIED_CONTEXT) {
      api.RegisterDependencyProvider("test2", &test2_provider);
    }

    api.LoadAllSchemas();
    EXPECT_EQ(test_data[i].expect_is_privilged,
              api.IsPrivileged(test_data[i].api_full_name)) << i;
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

  std::set<std::string> privileged_apis = extension_api->GetAPIsForContext(
      Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());

  std::set<std::string> unprivileged_apis = extension_api->GetAPIsForContext(
      Feature::UNBLESSED_EXTENSION_CONTEXT, extension.get(), GURL());

  std::set<std::string> content_script_apis = extension_api->GetAPIsForContext(
      Feature::CONTENT_SCRIPT_CONTEXT, extension.get(), GURL());

  // "storage" is completely unprivileged.
  EXPECT_EQ(1u, privileged_apis.count("storage"));
  EXPECT_EQ(1u, unprivileged_apis.count("storage"));
  EXPECT_EQ(1u, content_script_apis.count("storage"));

  // "extension" is partially unprivileged.
  EXPECT_EQ(1u, privileged_apis.count("extension"));
  EXPECT_EQ(1u, unprivileged_apis.count("extension"));
  EXPECT_EQ(1u, content_script_apis.count("extension"));

  // "history" is entirely privileged.
  EXPECT_EQ(1u, privileged_apis.count("history"));
  EXPECT_EQ(0u, unprivileged_apis.count("history"));
  EXPECT_EQ(0u, content_script_apis.count("history"));
}

TEST(ExtensionAPI, ExtensionWithDependencies) {
  // Extension with the "ttsEngine" permission but not the "tts" permission; it
  // must load TTS.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("ttsEngine");
    scoped_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    std::set<std::string> apis = api->GetAPIsForContext(
        Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());
    EXPECT_EQ(1u, apis.count("ttsEngine"));
    EXPECT_EQ(1u, apis.count("tts"));
  }

  // Conversely, extension with the "tts" permission but not the "ttsEngine"
  // permission shouldn't get the "ttsEngine" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("tts");
    scoped_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    std::set<std::string> apis = api->GetAPIsForContext(
        Feature::BLESSED_EXTENSION_CONTEXT, extension.get(), GURL());
    EXPECT_EQ(0u, apis.count("ttsEngine"));
    EXPECT_EQ(1u, apis.count("tts"));
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  std::set<std::string> apis =
      api->GetAPIsForContext(Feature::WEB_PAGE_CONTEXT, NULL, GURL(url));
  return apis.count(api_name);
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

  SimpleFeature* bookmarks =
      static_cast<SimpleFeature*>(api->GetFeature("bookmarks"));
  SimpleFeature* bookmarks_create =
      static_cast<SimpleFeature*>(api->GetFeature("bookmarks.create"));

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
  scoped_ptr<base::ListValue> schema1(new base::ListValue());
  base::DictionaryValue* feature_definition = new base::DictionaryValue();
  schema1->Append(feature_definition);
  feature_definition->SetString("namespace", "test");
  feature_definition->SetBoolean("uses_feature_system", true);

  scoped_ptr<base::ListValue> schema2(schema1->DeepCopy());

  base::ListValue* contexts = new base::ListValue();
  contexts->Append(new base::StringValue("content_script"));
  feature_definition->Set("contexts", contexts);

  struct {
    base::ListValue* schema;
    bool expect_success;
  } test_data[] = {
    { schema1.get(), true },
    { schema2.get(), false }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    std::string schema_source;
    base::JSONWriter::Write(test_data[i].schema, &schema_source);

    ExtensionAPI api;
    api.RegisterSchema("test", base::StringPiece(schema_source));
    api.LoadAllSchemas();

    Feature* feature = api.GetFeature("test");
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
  api.LoadAllSchemas();

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
