// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_web_contents_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

KeyedService* BuildAPI(content::BrowserContext* context) {
  return new DeveloperPrivateAPI(context);
}

}  // namespace

class DeveloperPrivateApiUnitTest : public ExtensionServiceTestBase {
 protected:
  DeveloperPrivateApiUnitTest() {}
  ~DeveloperPrivateApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<UIThreadExtensionFunction>& function,
                   const base::ListValue& args);

  // Loads an unpacked extension that is backed by a real directory, allowing
  // it to be reloaded.
  const Extension* LoadUnpackedExtension();

  // Tests a developer private function (T) that sets an extension pref, and
  // verifies it with |has_pref|.
  template<typename T>
  void TestExtensionPrefSetting(
      bool (*has_pref)(const std::string&, content::BrowserContext*));

  testing::AssertionResult TestPackExtensionFunction(
      const base::ListValue& args,
      api::developer_private::PackStatus expected_status,
      int expected_flags);

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  scoped_ptr<TestBrowserWindow> browser_window_;
  scoped_ptr<Browser> browser_;

  ScopedVector<TestExtensionDir> test_extension_dirs_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateApiUnitTest);
};

bool DeveloperPrivateApiUnitTest::RunFunction(
    const scoped_refptr<UIThreadExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(),
      make_scoped_ptr(args.DeepCopy()),
      browser(),
      extension_function_test_utils::NONE);
}

const Extension* DeveloperPrivateApiUnitTest::LoadUnpackedExtension() {
  const char kManifest[] =
      "{"
      " \"name\": \"foo\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2"
      "}";

  test_extension_dirs_.push_back(new TestExtensionDir);
  TestExtensionDir* dir = test_extension_dirs_.back();
  dir->WriteManifest(kManifest);

  // TODO(devlin): We should extract out methods to load an unpacked extension
  // synchronously. We do it in ExtensionBrowserTest, but that's not helpful
  // for unittests.
  TestExtensionRegistryObserver registry_observer(registry());
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(service()));
  installer->Load(dir->unpacked_path());
  base::FilePath extension_path =
      base::MakeAbsoluteFilePath(dir->unpacked_path());
  const Extension* extension = nullptr;
  do {
    extension = registry_observer.WaitForExtensionLoaded();
  } while (extension->path() != extension_path);
  // The fact that unpacked extensions get file access by default is an
  // irrelevant detail to these tests. Disable it.
  ExtensionPrefs::Get(browser_context())->SetAllowFileAccess(extension->id(),
                                                             false);
  return extension;
}

template<typename T>
void DeveloperPrivateApiUnitTest::TestExtensionPrefSetting(
    bool (*has_pref)(const std::string&, content::BrowserContext*)) {
  // Sadly, we need a "real" directory here, because toggling incognito causes
  // a reload (which needs a path).
  std::string extension_id = LoadUnpackedExtension()->id();

  scoped_refptr<UIThreadExtensionFunction> function(new T());

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  EXPECT_FALSE(has_pref(extension_id, profile()));

  // Pref-setting should require a user action.
  EXPECT_FALSE(RunFunction(function, enable_args));
  EXPECT_EQ(std::string("This action requires a user gesture."),
            function->GetError());

  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  function = new T();
  EXPECT_TRUE(RunFunction(function, enable_args));
  EXPECT_TRUE(has_pref(extension_id, profile()));

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);
  function = new T();
  EXPECT_TRUE(RunFunction(function, disable_args));
  EXPECT_FALSE(has_pref(extension_id, profile()));
}

testing::AssertionResult DeveloperPrivateApiUnitTest::TestPackExtensionFunction(
    const base::ListValue& args,
    api::developer_private::PackStatus expected_status,
    int expected_flags) {
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivatePackDirectoryFunction());
  if (!RunFunction(function, args))
    return testing::AssertionFailure() << "Could not run function.";

  // Extract the result. We don't have to test this here, since it's verified as
  // part of the general extension api system.
  const base::Value* response_value = nullptr;
  CHECK(function->GetResultList()->Get(0u, &response_value));
  scoped_ptr<api::developer_private::PackDirectoryResponse> response =
      api::developer_private::PackDirectoryResponse::FromValue(*response_value);
  CHECK(response);

  if (response->status != expected_status) {
    return testing::AssertionFailure() << "Expected status: " <<
        expected_status << ", found status: " << response->status <<
        ", message: " << response->message;
  }

  if (response->override_flags != expected_flags) {
    return testing::AssertionFailure() << "Expected flags: " <<
        expected_flags << ", found flags: " << response->override_flags;
  }

  return testing::AssertionSuccess();
}

void DeveloperPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));

  // Allow the API to be created.
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))->
      SetEventRouter(make_scoped_ptr(
          new EventRouter(profile(), ExtensionPrefs::Get(profile()))));
  DeveloperPrivateAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), &BuildAPI);
}

void DeveloperPrivateApiUnitTest::TearDown() {
  test_extension_dirs_.clear();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test developerPrivate.allowIncognito.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateAllowIncognito) {
  TestExtensionPrefSetting<api::DeveloperPrivateAllowIncognitoFunction>(
      &util::IsIncognitoEnabled);
}

// Test developerPrivate.reload.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateReload) {
  const Extension* extension = LoadUnpackedExtension();
  std::string extension_id = extension->id();
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateReloadFunction());
  base::ListValue reload_args;
  reload_args.AppendString(extension_id);

  TestExtensionRegistryObserver registry_observer(registry());
  EXPECT_TRUE(RunFunction(function, reload_args));
  const Extension* unloaded_extension =
      registry_observer.WaitForExtensionUnloaded();
  EXPECT_EQ(extension, unloaded_extension);
  const Extension* reloaded_extension =
      registry_observer.WaitForExtensionLoaded();
  EXPECT_EQ(extension_id, reloaded_extension->id());
}

// Test developerPrivate.allowFileAccess.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateAllowFileAccess) {
  TestExtensionPrefSetting<api::DeveloperPrivateAllowFileAccessFunction>(
      &util::AllowFileAccess);
}

// Test developerPrivate.packDirectory.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivatePackFunction) {
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);

  base::FilePath root_path = data_dir().AppendASCII("good_unpacked");
  base::FilePath crx_path = data_dir().AppendASCII("good_unpacked.crx");
  base::FilePath pem_path = data_dir().AppendASCII("good_unpacked.pem");

  // First, test a directory that should pack properly.
  base::ListValue pack_args;
  pack_args.AppendString(root_path.AsUTF8Unsafe());
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Should have created crx file and pem file.
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_TRUE(base::PathExists(pem_path));

  // Deliberately don't cleanup the files, and append the pem path.
  pack_args.AppendString(pem_path.AsUTF8Unsafe());

  // Try to pack again - we should get a warning abot overwriting the crx.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args,
      api::developer_private::PACK_STATUS_WARNING,
      ExtensionCreator::kOverwriteCRX));

  // Try to pack again, with the overwrite flag; this should succeed.
  pack_args.AppendInteger(ExtensionCreator::kOverwriteCRX);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Try to pack a final time when omitting (an existing) pem file. We should
  // get an error.
  base::DeleteFile(crx_path, false);
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the pem key argument.
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the flags argument.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_ERROR, 0));

  base::DeleteFile(crx_path, false);
  base::DeleteFile(pem_path, false);
}

// Test developerPrivate.choosePath.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateChoosePath) {
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(profile());

  base::FilePath expected_dir_path = data_dir().AppendASCII("good_unpacked");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_dir_path);

  // Try selecting a directory.
  base::ListValue choose_args;
  choose_args.AppendString("FOLDER");
  choose_args.AppendString("LOAD");
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateChoosePathFunction());
  function->SetRenderViewHost(web_contents->GetRenderViewHost());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  std::string path;
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_dir_path.AsUTF8Unsafe());

  // Try selecting a pem file.
  base::FilePath expected_file_path =
      data_dir().AppendASCII("good_unpacked.pem");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_file_path);
  choose_args.Clear();
  choose_args.AppendString("FILE");
  choose_args.AppendString("PEM");
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderViewHost(web_contents->GetRenderViewHost());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_file_path.AsUTF8Unsafe());

  // Try canceling the file dialog.
  api::EntryPicker::SkipPickerAndAlwaysCancelForTest();
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderViewHost(web_contents->GetRenderViewHost());
  EXPECT_FALSE(RunFunction(function, choose_args));
  EXPECT_EQ(std::string("File selection was canceled."), function->GetError());
}

// Test developerPrivate.loadUnpacked.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateLoadUnpacked) {
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(profile());

  base::FilePath path = data_dir().AppendASCII("good_unpacked");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a good extension (it should succeed, and the extension should
  // be added).
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateLoadUnpackedFunction());
  function->SetRenderViewHost(web_contents->GetRenderViewHost());
  ExtensionIdSet current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  // We should have added one new extension.
  ExtensionIdSet id_difference = base::STLSetDifference<ExtensionIdSet>(
      registry()->enabled_extensions().GetIDs(), current_ids);
  ASSERT_EQ(1u, id_difference.size());
  // The new extension should have the same path.
  EXPECT_EQ(
      path,
      registry()->enabled_extensions().GetByID(*id_difference.begin())->path());

  path = data_dir().AppendASCII("empty_manifest");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a bad extension (it should fail, and we should get an error).
  function = new api::DeveloperPrivateLoadUnpackedFunction();
  function->SetRenderViewHost(web_contents->GetRenderViewHost());
  base::ListValue unpacked_args;
  scoped_ptr<base::DictionaryValue> options(new base::DictionaryValue());
  options->SetBoolean("failQuietly", true);
  unpacked_args.Append(options.release());
  current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_FALSE(RunFunction(function, unpacked_args));
  EXPECT_EQ(manifest_errors::kManifestUnreadable, function->GetError());
  // We should have no new extensions installed.
  EXPECT_EQ(0u, base::STLSetDifference<ExtensionIdSet>(
                    registry()->enabled_extensions().GetIDs(),
                    current_ids).size());
}

// Test developerPrivate.requestFileSource.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateRequestFileSource) {
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  // Testing of this function seems light, but that's because it basically just
  // forwards to reading a file to a string, and highlighting it - both of which
  // are already tested separately.
  const Extension* extension = LoadUnpackedExtension();
  const char kErrorMessage[] = "Something went wrong";
  api::developer_private::RequestFileSourceProperties properties;
  properties.extension_id = extension->id();
  properties.path_suffix = "manifest.json";
  properties.message = kErrorMessage;
  properties.manifest_key.reset(new std::string("name"));

  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateRequestFileSourceFunction());
  base::ListValue file_source_args;
  file_source_args.Append(properties.ToValue().release());
  EXPECT_TRUE(RunFunction(function, file_source_args)) << function->GetError();

  const base::Value* response_value = nullptr;
  ASSERT_TRUE(function->GetResultList()->Get(0u, &response_value));
  scoped_ptr<api::developer_private::RequestFileSourceResponse> response =
      api::developer_private::RequestFileSourceResponse::FromValue(
          *response_value);
  EXPECT_FALSE(response->before_highlight.empty());
  EXPECT_EQ("\"name\": \"foo\"", response->highlight);
  EXPECT_FALSE(response->after_highlight.empty());
  EXPECT_EQ("foo: manifest.json", response->title);
  EXPECT_EQ(kErrorMessage, response->message);
}

// Test developerPrivate.getExtensionsInfo.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateGetExtensionsInfo) {
  // Enable error console for testing.
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  FeatureSwitch::ScopedOverride error_console_override(
      FeatureSwitch::error_console(), true);
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId(kName);
  DictionaryBuilder manifest;
  manifest.Set("name", kName)
          .Set("version", kVersion)
          .Set("manifest_version", 2)
          .Set("description", "an extension")
          .Set("permissions", ListBuilder().Append("file://*/*"));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(manifest)
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

  // It's not feasible to validate every field here, because that would be
  // a duplication of the logic in the method itself. Instead, test a handful
  // of fields, and, more importantly, check that we return something reasonable
  // (which is done through the serialization/deserialization that happens).
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateGetExtensionsInfoFunction());
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  const base::ListValue* results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  const base::ListValue* list = nullptr;
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  const base::Value* value = nullptr;
  ASSERT_TRUE(list->Get(0u, &value));
  scoped_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*value);
  ASSERT_TRUE(info);
  EXPECT_EQ(kName, info->name);
  EXPECT_EQ(id, info->id);
  EXPECT_EQ(kVersion, info->version);
  EXPECT_EQ(api::developer_private::EXTENSION_STATE_ENABLED, info->state);
  EXPECT_EQ(api::developer_private::EXTENSION_TYPE_EXTENSION, info->type);
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
  EXPECT_TRUE(info->incognito_access.is_enabled);
  EXPECT_FALSE(info->incognito_access.is_active);
  ASSERT_EQ(1u, info->runtime_errors.size());
  const api::developer_private::RuntimeError& runtime_error =
      *info->runtime_errors[0];
  EXPECT_EQ(extension->id(), runtime_error.extension_id);
  EXPECT_EQ(api::developer_private::ERROR_TYPE_RUNTIME, runtime_error.type);
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_ERROR,
            runtime_error.severity);
  EXPECT_EQ(1u, runtime_error.stack_trace.size());
  ASSERT_EQ(1u, info->manifest_errors.size());
  const api::developer_private::ManifestError& manifest_error =
      *info->manifest_errors[0];
  EXPECT_EQ(extension->id(), manifest_error.extension_id);

  // As a sanity check, also run the GetItemsInfo and make sure it returns a
  // sane value.
  function = new api::DeveloperPrivateGetItemsInfoFunction();
  base::ListValue args;
  args.AppendBoolean(false);
  args.AppendBoolean(false);
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  ASSERT_TRUE(list->Get(0u, &value));
  scoped_ptr<api::developer_private::ItemInfo> item_info =
      api::developer_private::ItemInfo::FromValue(*value);
  ASSERT_TRUE(item_info);
}

}  // namespace extensions
