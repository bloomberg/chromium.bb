// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {

const char kAllHostsPermission[] = "*://*/*";

scoped_ptr<base::DictionaryValue> DeserializeJSONTestData(
    const base::FilePath& path,
    std::string *error) {
  JSONFileValueDeserializer deserializer(path);
  return base::DictionaryValue::From(deserializer.Deserialize(nullptr, error));
}

}  // namespace

class ExtensionInfoGeneratorUnitTest : public ExtensionServiceTestBase {
 public:
  ExtensionInfoGeneratorUnitTest() {}
  ~ExtensionInfoGeneratorUnitTest() override {}

 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  void OnInfosGenerated(linked_ptr<developer::ExtensionInfo>* info_out,
                        const ExtensionInfoGenerator::ExtensionInfoList& list) {
    EXPECT_EQ(1u, list.size());
    if (!list.empty())
      *info_out = list[0];
    quit_closure_.Run();
    quit_closure_.Reset();
  }

  scoped_ptr<developer::ExtensionInfo> GenerateExtensionInfo(
      const std::string& extension_id) {
    linked_ptr<developer::ExtensionInfo> info;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    scoped_ptr<ExtensionInfoGenerator> generator(
        new ExtensionInfoGenerator(browser_context()));
    generator->CreateExtensionInfo(
        extension_id,
        base::Bind(&ExtensionInfoGeneratorUnitTest::OnInfosGenerated,
                   base::Unretained(this),
                   base::Unretained(&info)));
    run_loop.Run();
    return make_scoped_ptr(info.release());
  }

  const scoped_refptr<const Extension> CreateExtension(
      const std::string& name,
      ListBuilder& permissions) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder().SetManifest(
                               DictionaryBuilder()
                                   .Set("name", name)
                                   .Set("description", "an extension")
                                   .Set("manifest_version", 2)
                                   .Set("version", "1.0.0")
                                   .Set("permissions", permissions))
                          .SetLocation(Manifest::INTERNAL)
                          .SetID(kId)
                          .Build();

    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    PermissionsUpdater(profile()).InitializePermissions(extension.get());
    return extension;
  }

  scoped_ptr<developer::ExtensionInfo> CreateExtensionInfoFromPath(
      const base::FilePath& extension_path,
      Manifest::Location location) {
    std::string error;

    base::FilePath manifest_path = extension_path.Append(kManifestFilename);
    scoped_ptr<base::DictionaryValue> extension_data =
        DeserializeJSONTestData(manifest_path, &error);
    EXPECT_EQ(std::string(), error);

    scoped_refptr<Extension> extension(Extension::Create(
        extension_path, location, *extension_data, Extension::REQUIRE_KEY,
        &error));
    CHECK(extension.get());
    service()->AddExtension(extension.get());
    EXPECT_EQ(std::string(), error);

    return GenerateExtensionInfo(extension->id());
  }

  void CompareExpectedAndActualOutput(
      const base::FilePath& extension_path,
      const InspectableViewsFinder::ViewList& views,
      const base::FilePath& expected_output_path) {
    std::string error;
    scoped_ptr<base::DictionaryValue> expected_output_data(
        DeserializeJSONTestData(expected_output_path, &error));
    EXPECT_EQ(std::string(), error);

    // Produce test output.
    scoped_ptr<developer::ExtensionInfo> info =
        CreateExtensionInfoFromPath(extension_path, Manifest::INVALID_LOCATION);
    info->views = views;
    scoped_ptr<base::DictionaryValue> actual_output_data = info->ToValue();
    ASSERT_TRUE(actual_output_data);

    // Compare the outputs.
    // Ignore unknown fields in the actual output data.
    std::string paths_details = " - expected (" +
        expected_output_path.MaybeAsASCII() + ") vs. actual (" +
        extension_path.MaybeAsASCII() + ")";
    std::string expected_string;
    std::string actual_string;
    for (base::DictionaryValue::Iterator field(*expected_output_data);
         !field.IsAtEnd(); field.Advance()) {
      const base::Value& expected_value = field.value();
      base::Value* actual_value = nullptr;
      EXPECT_TRUE(actual_output_data->Get(field.key(), &actual_value)) <<
          field.key() + " is missing" + paths_details;
      if (!actual_value)
        continue;
      if (!actual_value->Equals(&expected_value)) {
        base::JSONWriter::Write(expected_value, &expected_string);
        base::JSONWriter::Write(*actual_value, &actual_string);
        EXPECT_EQ(expected_string, actual_string) <<
            field.key() << paths_details;
      }
    }
  }

 private:
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoGeneratorUnitTest);
};

// Test some of the basic fields.
TEST_F(ExtensionInfoGeneratorUnitTest, BasicInfoTest) {
  // Enable error console for testing.
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  FeatureSwitch::ScopedOverride error_console_override(
      FeatureSwitch::error_console(), true);
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId("alpha");
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder().Set("name", kName)
                         .Set("version", kVersion)
                         .Set("manifest_version", 2)
                         .Set("description", "an extension")
                         .Set("permissions",
                              ListBuilder().Append("file://*/*")).Build();
  scoped_ptr<base::DictionaryValue> manifest_copy(manifest->DeepCopy());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(manifest.Pass())
                        .SetLocation(Manifest::UNPACKED)
                        .SetPath(data_dir())
                        .SetID(id)
                        .Build();
  service()->AddExtension(extension.get());
  ErrorConsole* error_console = ErrorConsole::Get(profile());
  error_console->ReportError(
      make_scoped_ptr(new RuntimeError(
          extension->id(),
          false,
          base::UTF8ToUTF16("source"),
          base::UTF8ToUTF16("message"),
          StackTrace(1, StackFrame(1,
                                   1,
                                   base::UTF8ToUTF16("source"),
                                   base::UTF8ToUTF16("function"))),
          GURL("url"),
          logging::LOG_ERROR,
          1,
          1)));
  error_console->ReportError(
      make_scoped_ptr(new ManifestError(extension->id(),
                                        base::UTF8ToUTF16("message"),
                                        base::UTF8ToUTF16("key"),
                                        base::string16())));
  error_console->ReportError(
      make_scoped_ptr(new RuntimeError(
          extension->id(),
          false,
          base::UTF8ToUTF16("source"),
          base::UTF8ToUTF16("message"),
          StackTrace(1, StackFrame(1,
                                   1,
                                   base::UTF8ToUTF16("source"),
                                   base::UTF8ToUTF16("function"))),
          GURL("url"),
          logging::LOG_VERBOSE,
          1,
          1)));

  // It's not feasible to validate every field here, because that would be
  // a duplication of the logic in the method itself. Instead, test a handful
  // of fields for sanity.
  scoped_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info.get());
  EXPECT_EQ(kName, info->name);
  EXPECT_EQ(id, info->id);
  EXPECT_EQ(kVersion, info->version);
  EXPECT_EQ(info->location, developer::LOCATION_UNPACKED);
  ASSERT_TRUE(info->path);
  EXPECT_EQ(data_dir(), base::FilePath::FromUTF8Unsafe(*info->path));
  EXPECT_EQ(api::developer_private::EXTENSION_STATE_ENABLED, info->state);
  EXPECT_EQ(api::developer_private::EXTENSION_TYPE_EXTENSION, info->type);
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
  EXPECT_TRUE(info->incognito_access.is_enabled);
  EXPECT_FALSE(info->incognito_access.is_active);
  ASSERT_EQ(2u, info->runtime_errors.size());
  const api::developer_private::RuntimeError& runtime_error =
      *info->runtime_errors[0];
  EXPECT_EQ(extension->id(), runtime_error.extension_id);
  EXPECT_EQ(api::developer_private::ERROR_TYPE_RUNTIME, runtime_error.type);
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_ERROR,
            runtime_error.severity);
  EXPECT_EQ(1u, runtime_error.stack_trace.size());
  ASSERT_EQ(1u, info->manifest_errors.size());
  const api::developer_private::RuntimeError& runtime_error_verbose =
      *info->runtime_errors[1];
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_LOG,
            runtime_error_verbose.severity);
  const api::developer_private::ManifestError& manifest_error =
      *info->manifest_errors[0];
  EXPECT_EQ(extension->id(), manifest_error.extension_id);

  // Test an extension that isn't unpacked.
  manifest_copy->SetString("update_url",
                           "https://clients2.google.com/service/update2/crx");
  id = crx_file::id_util::GenerateId("beta");
  extension = ExtensionBuilder().SetManifest(manifest_copy.Pass())
                                .SetLocation(Manifest::EXTERNAL_PREF)
                                .SetID(id)
                                .Build();
  service()->AddExtension(extension.get());
  info = GenerateExtensionInfo(extension->id());
  EXPECT_EQ(developer::LOCATION_THIRD_PARTY, info->location);
  EXPECT_FALSE(info->path);
}

// Test three generated json outputs.
TEST_F(ExtensionInfoGeneratorUnitTest, GenerateExtensionsJSONData) {
  // Test Extension1
  base::FilePath extension_path =
      data_dir().AppendASCII("good")
                .AppendASCII("Extensions")
                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                .AppendASCII("1.0.0.0");

  InspectableViewsFinder::ViewList views;
  views.push_back(InspectableViewsFinder::ConstructView(
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/bar.html"),
      42, 88, true, false, VIEW_TYPE_TAB_CONTENTS));
  views.push_back(InspectableViewsFinder::ConstructView(
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/dog.html"),
      0, 0, false, true, VIEW_TYPE_TAB_CONTENTS));

  base::FilePath expected_outputs_path =
      data_dir().AppendASCII("api_test")
                .AppendASCII("developer")
                .AppendASCII("generated_output");

  CompareExpectedAndActualOutput(
      extension_path,
      views,
      expected_outputs_path.AppendASCII(
          "behllobkkfkfnphdnhnkndlbkcpglgmj.json"));

#if !defined(OS_CHROMEOS)
  // Test Extension2
  extension_path = data_dir().AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("hpiknbiabeeppbpihjehijgoemciehgk")
                             .AppendASCII("2");

  // It's OK to have duplicate URLs, so long as the IDs are different.
  views[0]->url =
      "chrome-extension://hpiknbiabeeppbpihjehijgoemciehgk/bar.html";
  views[1]->url = views[0]->url;

  CompareExpectedAndActualOutput(
      extension_path,
      views,
      expected_outputs_path.AppendASCII(
          "hpiknbiabeeppbpihjehijgoemciehgk.json"));
#endif

  // Test Extension3
  extension_path = data_dir().AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                             .AppendASCII("1.0");
  views.clear();

  CompareExpectedAndActualOutput(
      extension_path,
      views,
      expected_outputs_path.AppendASCII(
          "bjafgdebaacbbbecmhlhpofkepfkgcpa.json"));
}

// Test that the all_urls checkbox only shows up for extensions that want all
// urls, and only when the switch is on.
TEST_F(ExtensionInfoGeneratorUnitTest, ExtensionInfoRunOnAllUrls) {
  // Start with the switch enabled.
  scoped_ptr<FeatureSwitch::ScopedOverride> enable_scripts_switch(
      new FeatureSwitch::ScopedOverride(
          FeatureSwitch::scripts_require_action(), true));
  // Two extensions - one with all urls, one without.
  scoped_refptr<const Extension> all_urls_extension = CreateExtension(
      "all_urls", ListBuilder().Append(kAllHostsPermission).Pass());
  scoped_refptr<const Extension> no_urls_extension =
      CreateExtension("no urls", ListBuilder().Pass());

  scoped_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(all_urls_extension->id());

  // The extension should want all urls, but not currently have it.
  EXPECT_TRUE(info->run_on_all_urls.is_enabled);
  EXPECT_FALSE(info->run_on_all_urls.is_active);

  // Give the extension all urls.
  util::SetAllowedScriptingOnAllUrls(all_urls_extension->id(), profile(), true);

  // Now the extension should both want and have all urls.
  info = GenerateExtensionInfo(all_urls_extension->id());
  EXPECT_TRUE(info->run_on_all_urls.is_enabled);
  EXPECT_TRUE(info->run_on_all_urls.is_active);

  // The other extension should neither want nor have all urls.
  info = GenerateExtensionInfo(no_urls_extension->id());
  EXPECT_FALSE(info->run_on_all_urls.is_enabled);
  EXPECT_FALSE(info->run_on_all_urls.is_active);

  // Revoke the first extension's permissions.
  util::SetAllowedScriptingOnAllUrls(
      all_urls_extension->id(), profile(), false);

  // Turn off the switch and load another extension (so permissions are
  // re-initialized).
  enable_scripts_switch.reset();

  // Since the extension doesn't have access to all urls (but normally would),
  // the extension should have the "want" flag even with the switch off.
  info = GenerateExtensionInfo(all_urls_extension->id());
  EXPECT_TRUE(info->run_on_all_urls.is_enabled);
  EXPECT_FALSE(info->run_on_all_urls.is_active);

  // If we grant the extension all urls, then the checkbox should still be
  // there, since it has an explicitly-set user preference.
  util::SetAllowedScriptingOnAllUrls(all_urls_extension->id(), profile(), true);
  info = GenerateExtensionInfo(all_urls_extension->id());
  EXPECT_TRUE(info->run_on_all_urls.is_enabled);
  EXPECT_TRUE(info->run_on_all_urls.is_active);

  // Load another extension with all urls (so permissions get re-init'd).
  all_urls_extension = CreateExtension(
      "all_urls_II", ListBuilder().Append(kAllHostsPermission).Pass());

  // Even though the extension has all_urls permission, the checkbox shouldn't
  // show up without the switch.
  info = GenerateExtensionInfo(all_urls_extension->id());
  EXPECT_FALSE(info->run_on_all_urls.is_enabled);
  EXPECT_TRUE(info->run_on_all_urls.is_active);
}

}  // namespace extensions
