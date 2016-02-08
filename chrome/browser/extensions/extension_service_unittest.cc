// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/value_builder.h"
#include "gpu/config/gpu_info.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/database/database_identifier.h"
#include "sync/api/string_ordinal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

// The blacklist tests rely on the safe-browsing database.
#if defined(SAFE_BROWSING_DB_LOCAL)
#define ENABLE_BLACKLIST_TESTS
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;
using content::IndexedDBContext;
using content::PluginService;
using extensions::APIPermission;
using extensions::APIPermissionSet;
using extensions::AppSorting;
using extensions::Blacklist;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionCreator;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionResource;
using extensions::ExtensionSystem;
using extensions::ExternalInstallError;
using extensions::ExternalInstallInfoFile;
using extensions::ExternalInstallInfoUpdateUrl;
using extensions::ExternalProviderInterface;
using extensions::FakeSafeBrowsingDatabaseManager;
using extensions::FeatureSwitch;
using extensions::Manifest;
using extensions::PermissionSet;
using extensions::TestExtensionSystem;
using extensions::UnloadedExtensionInfo;
using extensions::URLPatternSet;

namespace keys = extensions::manifest_keys;

namespace {

// Extension ids used during testing.
const char good0[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const char good1[] = "hpiknbiabeeppbpihjehijgoemciehgk";
const char good2[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
const char all_zero[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char good2048[] = "nmgjhmhbleinmjpbdhgajfjkbijcmgbh";
const char good_crx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char minimal_platform_app_crx[] = "jjeoclcdfjddkdjokiejckgcildcflpp";
const char hosted_app[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char page_action[] = "obcimlgaoabeegjmmpldobjndiealpln";
const char theme_crx[] = "iamefpfkojoapidjnbafmgkgncegbkad";
const char theme2_crx[] = "pjpgmfcmabopnnfonnhmdjglfpjjfkbf";
const char permissions_crx[] = "eagpmdpfmaekmmcejjbmjoecnejeiiin";
const char updates_from_webstore[] = "akjooamlhcgeopfifcmlggaebeocgokj";
const char permissions_blocklist[] = "noffkehfcaggllbcojjbopcmlhcnhcdn";

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

base::FilePath GetTemporaryFile() {
  base::FilePath temp_file;
  CHECK(base::CreateTemporaryFile(&temp_file));
  return temp_file;
}

bool WaitForCountNotificationsCallback(int *count) {
  return --(*count) == 0;
}

bool HasExternalInstallErrors(ExtensionService* service) {
  return !service->external_install_manager()->GetErrorsForTesting().empty();
}

bool HasExternalInstallBubble(ExtensionService* service) {
  std::vector<ExternalInstallError*> errors =
      service->external_install_manager()->GetErrorsForTesting();
  auto found = std::find_if(
      errors.begin(), errors.end(),
      [](const ExternalInstallError* error) {
    return error->alert_type() == ExternalInstallError::BUBBLE_ALERT;
  });
  return found != errors.end();
}

}  // namespace

class MockExtensionProvider : public extensions::ExternalProviderInterface {
 public:
  MockExtensionProvider(
      VisitorInterface* visitor,
      Manifest::Location location)
    : location_(location), visitor_(visitor), visit_count_(0) {
  }

  ~MockExtensionProvider() override {}

  void UpdateOrAddExtension(const std::string& id,
                            const std::string& version,
                            const base::FilePath& path) {
    extension_map_[id] = std::make_pair(version, path);
  }

  void RemoveExtension(const std::string& id) {
    extension_map_.erase(id);
  }

  // ExternalProvider implementation:
  void VisitRegisteredExtension() override {
    visit_count_++;
    for (DataMap::const_iterator i = extension_map_.begin();
         i != extension_map_.end(); ++i) {
      scoped_ptr<Version> version(new Version(i->second.first));

      scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
          i->first, std::move(version), i->second.second, location_,
          Extension::NO_FLAGS, false, false));
      visitor_->OnExternalExtensionFileFound(*info);
    }
    visitor_->OnExternalProviderReady(this);
  }

  bool HasExtension(const std::string& id) const override {
    return extension_map_.find(id) != extension_map_.end();
  }

  bool GetExtensionDetails(const std::string& id,
                           Manifest::Location* location,
                           scoped_ptr<Version>* version) const override {
    DataMap::const_iterator it = extension_map_.find(id);
    if (it == extension_map_.end())
      return false;

    if (version)
      version->reset(new Version(it->second.first));

    if (location)
      *location = location_;

    return true;
  }

  bool IsReady() const override { return true; }

  void ServiceShutdown() override {}

  int visit_count() const { return visit_count_; }
  void set_visit_count(int visit_count) {
    visit_count_ = visit_count;
  }

 private:
  typedef std::map< std::string, std::pair<std::string, base::FilePath> >
      DataMap;
  DataMap extension_map_;
  Manifest::Location location_;
  VisitorInterface* visitor_;

  // visit_count_ tracks the number of calls to VisitRegisteredExtension().
  // Mutable because it must be incremented on each call to
  // VisitRegisteredExtension(), which must be a const method to inherit
  // from the class being mocked.
  mutable int visit_count_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionProvider);
};

class MockProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  // The provider will return |fake_base_path| from
  // GetBaseCrxFilePath().  User can test the behavior with
  // and without an empty path using this parameter.
  explicit MockProviderVisitor(base::FilePath fake_base_path)
      : ids_found_(0),
        fake_base_path_(fake_base_path),
        expected_creation_flags_(Extension::NO_FLAGS) {
    profile_.reset(new TestingProfile);
  }

  MockProviderVisitor(base::FilePath fake_base_path,
                      int expected_creation_flags)
      : ids_found_(0),
        fake_base_path_(fake_base_path),
        expected_creation_flags_(expected_creation_flags) {
    profile_.reset(new TestingProfile);
  }

  int Visit(const std::string& json_data) {
    return Visit(json_data, Manifest::EXTERNAL_PREF,
                 Manifest::EXTERNAL_PREF_DOWNLOAD);
  }

  int Visit(const std::string& json_data,
            Manifest::Location crx_location,
            Manifest::Location download_location) {
    crx_location_ = crx_location;
    // Give the test json file to the provider for parsing.
    provider_.reset(new extensions::ExternalProviderImpl(
        this, new extensions::ExternalTestingLoader(json_data, fake_base_path_),
        profile_.get(), crx_location, download_location, Extension::NO_FLAGS));

    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    prefs_ = GetDictionaryFromJSON(json_data);

    // Reset our counter.
    ids_found_ = 0;
    // Ask the provider to look up all extensions and return them.
    provider_->VisitRegisteredExtension();

    return ids_found_;
  }

  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    EXPECT_EQ(expected_creation_flags_, info.creation_flags);

    ++ids_found_;
    base::DictionaryValue* pref;
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(prefs_->GetDictionary(info.extension_id, &pref))
        << "Got back ID (" << info.extension_id.c_str()
        << ") we weren't expecting";

    EXPECT_TRUE(info.path.IsAbsolute());
    if (!fake_base_path_.empty())
      EXPECT_TRUE(fake_base_path_.IsParent(info.path));

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(info.extension_id));

      // Ask provider if the extension we got back is registered.
      Manifest::Location location = Manifest::INVALID_LOCATION;
      scoped_ptr<Version> v1;
      base::FilePath crx_path;

      EXPECT_TRUE(provider_->GetExtensionDetails(info.extension_id, NULL, &v1));
      EXPECT_STREQ(info.version->GetString().c_str(), v1->GetString().c_str());

      scoped_ptr<Version> v2;
      EXPECT_TRUE(
          provider_->GetExtensionDetails(info.extension_id, &location, &v2));
      EXPECT_STREQ(info.version->GetString().c_str(), v1->GetString().c_str());
      EXPECT_STREQ(info.version->GetString().c_str(), v2->GetString().c_str());
      EXPECT_EQ(crx_location_, location);

      // Remove it so we won't count it ever again.
      prefs_->Remove(info.extension_id, NULL);
    }
    return true;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool is_initial_load) override {
    ++ids_found_;
    base::DictionaryValue* pref;
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(prefs_->GetDictionary(info.extension_id, &pref))
        << L"Got back ID (" << info.extension_id.c_str()
        << ") we weren't expecting";
    EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, info.download_location);

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(info.extension_id));

      // External extensions with update URLs do not have versions.
      scoped_ptr<Version> v1;
      Manifest::Location location1 = Manifest::INVALID_LOCATION;
      EXPECT_TRUE(
          provider_->GetExtensionDetails(info.extension_id, &location1, &v1));
      EXPECT_FALSE(v1.get());
      EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, location1);

      std::string parsed_install_parameter;
      pref->GetString("install_parameter", &parsed_install_parameter);
      EXPECT_EQ(parsed_install_parameter, info.install_parameter);

      // Remove it so we won't count it again.
      prefs_->Remove(info.extension_id, NULL);
    }
    return true;
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const ScopedVector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const ScopedVector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "MockProviderVisitor does not provide incremental updates,"
                     " use MockUpdateProviderVisitor instead.";
  }

  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {
    EXPECT_EQ(provider, provider_.get());
    EXPECT_TRUE(provider->IsReady());
  }

  Profile* profile() { return profile_.get(); }

 protected:
  scoped_ptr<extensions::ExternalProviderImpl> provider_;

  scoped_ptr<base::DictionaryValue> GetDictionaryFromJSON(
      const std::string& json_data) {
    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    JSONStringValueDeserializer deserializer(json_data);
    scoped_ptr<base::Value> json_value = deserializer.Deserialize(NULL, NULL);

    if (!json_value || !json_value->IsType(base::Value::TYPE_DICTIONARY)) {
      ADD_FAILURE() << "Unable to deserialize json data";
      return scoped_ptr<base::DictionaryValue>();
    } else {
      return base::DictionaryValue::From(std::move(json_value));
    }
  }

 private:
  int ids_found_;
  base::FilePath fake_base_path_;
  int expected_creation_flags_;
  Manifest::Location crx_location_;
  scoped_ptr<base::DictionaryValue> prefs_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(MockProviderVisitor);
};

// Mock provider that can simulate incremental update like
// ExternalRegistryLoader.
class MockUpdateProviderVisitor : public MockProviderVisitor {
 public:
  // The provider will return |fake_base_path| from
  // GetBaseCrxFilePath().  User can test the behavior with
  // and without an empty path using this parameter.
  explicit MockUpdateProviderVisitor(base::FilePath fake_base_path)
      : MockProviderVisitor(fake_base_path) {}

  void VisitDueToUpdate(const std::string& json_data) {
    update_url_extension_ids_.clear();
    file_extension_ids_.clear();
    removed_extension_ids_.clear();

    scoped_ptr<base::DictionaryValue> new_prefs =
        GetDictionaryFromJSON(json_data);
    if (!new_prefs)
      return;
    provider_->UpdatePrefs(new_prefs.release());
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const ScopedVector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const ScopedVector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    for (const auto& extension_info : update_url_extensions)
      update_url_extension_ids_.insert(extension_info->extension_id);
    EXPECT_EQ(update_url_extension_ids_.size(), update_url_extensions.size());

    for (const auto& extension_info : file_extensions)
      file_extension_ids_.insert(extension_info->extension_id);
    EXPECT_EQ(file_extension_ids_.size(), file_extensions.size());

    for (const auto& extension_id : removed_extensions)
      removed_extension_ids_.insert(extension_id);
  }

  size_t GetUpdateURLExtensionCount() {
    return update_url_extension_ids_.size();
  }
  size_t GetFileExtensionCount() { return file_extension_ids_.size(); }
  size_t GetRemovedExtensionCount() { return removed_extension_ids_.size(); }

  bool HasSeenUpdateWithUpdateUrl(const std::string& extension_id) {
    return update_url_extension_ids_.count(extension_id) > 0u;
  }
  bool HasSeenUpdateWithFile(const std::string& extension_id) {
    return file_extension_ids_.count(extension_id) > 0u;
  }
  bool HasSeenRemoval(const std::string& extension_id) {
    return removed_extension_ids_.count(extension_id) > 0u;
  }

 private:
  std::set<std::string> update_url_extension_ids_;
  std::set<std::string> file_extension_ids_;
  std::set<std::string> removed_extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(MockUpdateProviderVisitor);
};

class ExtensionServiceTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  ExtensionServiceTest() {
    // The extension subsystem posts logging tasks to be done after
    // browser startup. There's no StartupObserver as there normally
    // would be since we're in a unit test, so we have to explicitly
    // note tasks should be processed.
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  }

  void AddMockExternalProvider(
      extensions::ExternalProviderInterface* provider) {
    service()->AddProviderForTesting(provider);
  }

 protected:
  // Paths to some of the fake extensions.
  base::FilePath good1_path() {
    return data_dir()
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII(good1)
        .AppendASCII("2");
  }

  base::FilePath good2_path() {
    return data_dir()
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII(good2)
        .AppendASCII("1.0");
  }

  void TestExternalProvider(MockExtensionProvider* provider,
                            Manifest::Location location);

  void BlackListWebGL() {
    static const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"features\": [\"webgl\"]\n"
      "    }\n"
      "  ]\n"
      "}";
    gpu::GPUInfo gpu_info;
    content::GpuDataManager::GetInstance()->InitializeForTesting(
        json_blacklist, gpu_info);
  }

  // Grants all optional permissions stated in manifest to active permission
  // set for extension |id|.
  void GrantAllOptionalPermissions(const std::string& id) {
    const Extension* extension = service()->GetInstalledExtension(id);
    const PermissionSet& all_optional_permissions =
        extensions::PermissionsParser::GetOptionalPermissions(extension);
    extensions::PermissionsUpdater perms_updater(profile());
    perms_updater.AddPermissions(extension, all_optional_permissions);
  }

  testing::AssertionResult IsBlocked(const std::string& id) {
    scoped_ptr<extensions::ExtensionSet> all_unblocked_extensions =
        registry()->GenerateInstalledExtensionsSet(
            ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::BLOCKED);
    if (all_unblocked_extensions.get()->Contains(id))
      return testing::AssertionFailure() << id << " is still unblocked!";
    if (!registry()->blocked_extensions().Contains(id))
      return testing::AssertionFailure() << id << " is not blocked!";
    return testing::AssertionSuccess();
  }

  // Helper method to test that an extension moves through being blocked and
  // unblocked as appropriate for its type.
  void AssertExtensionBlocksAndUnblocks(
      bool should_block, const std::string extension_id) {
    // Assume we start in an unblocked state.
    EXPECT_FALSE(IsBlocked(extension_id));

    // Block the extensions.
    service()->BlockAllExtensions();
    base::RunLoop().RunUntilIdle();

    if (should_block)
      ASSERT_TRUE(IsBlocked(extension_id));
    else
      ASSERT_FALSE(IsBlocked(extension_id));

    service()->UnblockAllExtensions();
    base::RunLoop().RunUntilIdle();

    ASSERT_FALSE(IsBlocked(extension_id));
  }

  bool IsPrefExist(const std::string& extension_id,
                   const std::string& pref_path) {
    const base::DictionaryValue* dict =
        profile()->GetPrefs()->GetDictionary("extensions.settings");
    if (dict == NULL) return false;
    const base::DictionaryValue* pref = NULL;
    if (!dict->GetDictionary(extension_id, &pref)) {
      return false;
    }
    if (pref == NULL) {
      return false;
    }
    bool val;
    if (!pref->GetBoolean(pref_path, &val)) {
      return false;
    }
    return true;
  }

  void SetPref(const std::string& extension_id,
               const std::string& pref_path,
               base::Value* value,
               const std::string& msg) {
    DictionaryPrefUpdate update(profile()->GetPrefs(), "extensions.settings");
    base::DictionaryValue* dict = update.Get();
    ASSERT_TRUE(dict != NULL) << msg;
    base::DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    pref->Set(pref_path, value);
  }

  void SetPrefInteg(const std::string& extension_id,
                    const std::string& pref_path,
                    int value) {
    std::string msg = " while setting: ";
    msg += extension_id;
    msg += " ";
    msg += pref_path;
    msg += " = ";
    msg += base::IntToString(value);

    SetPref(extension_id, pref_path, new base::FundamentalValue(value), msg);
  }

  void SetPrefBool(const std::string& extension_id,
                   const std::string& pref_path,
                   bool value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;
    msg += " = ";
    msg += (value ? "true" : "false");

    SetPref(extension_id, pref_path, new base::FundamentalValue(value), msg);
  }

  void ClearPref(const std::string& extension_id,
                 const std::string& pref_path) {
    std::string msg = " while clearing: ";
    msg += extension_id + " " + pref_path;

    DictionaryPrefUpdate update(profile()->GetPrefs(), "extensions.settings");
    base::DictionaryValue* dict = update.Get();
    ASSERT_TRUE(dict != NULL) << msg;
    base::DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    pref->Remove(pref_path, NULL);
  }

  void SetPrefStringSet(const std::string& extension_id,
                        const std::string& pref_path,
                        const std::set<std::string>& value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;

    base::ListValue* list_value = new base::ListValue();
    for (std::set<std::string>::const_iterator iter = value.begin();
         iter != value.end(); ++iter)
      list_value->Append(new base::StringValue(*iter));

    SetPref(extension_id, pref_path, list_value, msg);
  }

  void InitPluginService() {
#if defined(ENABLE_PLUGINS)
    PluginService::GetInstance()->Init();
#endif
  }

  void InitializeEmptyExtensionServiceWithTestingPrefs() {
    ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.pref_file = base::FilePath();
    InitializeExtensionService(params);
  }

  extensions::ManagementPolicy* GetManagementPolicy() {
    return ExtensionSystem::Get(browser_context())->management_policy();
  }

  ExternalInstallError* GetError(const std::string& extension_id) {
    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    auto found = std::find_if(
        errors.begin(), errors.end(),
        [&extension_id](const ExternalInstallError* error) {
      return error->extension_id() == extension_id;
    });
    return found == errors.end() ? nullptr : *found;
  }

  typedef extensions::ExtensionManagementPrefUpdater<
      syncable_prefs::TestingPrefServiceSyncable> ManagementPrefUpdater;
};

// Receives notifications from a PackExtensionJob, indicating either that
// packing succeeded or that there was some error.
class PackExtensionTestClient : public extensions::PackExtensionJob::Client {
 public:
  PackExtensionTestClient(const base::FilePath& expected_crx_path,
                          const base::FilePath& expected_private_key_path);
  void OnPackSuccess(const base::FilePath& crx_path,
                     const base::FilePath& private_key_path) override;
  void OnPackFailure(const std::string& error_message,
                     ExtensionCreator::ErrorType type) override;

 private:
  const base::FilePath expected_crx_path_;
  const base::FilePath expected_private_key_path_;
  DISALLOW_COPY_AND_ASSIGN(PackExtensionTestClient);
};

PackExtensionTestClient::PackExtensionTestClient(
    const base::FilePath& expected_crx_path,
    const base::FilePath& expected_private_key_path)
    : expected_crx_path_(expected_crx_path),
      expected_private_key_path_(expected_private_key_path) {}

// If packing succeeded, we make sure that the package names match our
// expectations.
void PackExtensionTestClient::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& private_key_path) {
  // We got the notification and processed it; we don't expect any further tasks
  // to be posted to the current thread, so we should stop blocking and continue
  // on with the rest of the test.
  // This call to |Quit()| matches the call to |Run()| in the
  // |PackPunctuatedExtension| test.
  base::MessageLoop::current()->QuitWhenIdle();
  EXPECT_EQ(expected_crx_path_.value(), crx_path.value());
  EXPECT_EQ(expected_private_key_path_.value(), private_key_path.value());
  ASSERT_TRUE(base::PathExists(private_key_path));
}

// The tests are designed so that we never expect to see a packing error.
void PackExtensionTestClient::OnPackFailure(const std::string& error_message,
                                            ExtensionCreator::ErrorType type) {
  if (type == ExtensionCreator::kCRXExists)
     FAIL() << "Packing should not fail.";
  else
     FAIL() << "Existing CRX should have been overwritten.";
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectorySuccess) {
  InitPluginService();
  InitializeGoodInstalledExtensionService();
  service()->Init();

  uint32_t expected_num_extensions = 3u;
  ASSERT_EQ(expected_num_extensions, loaded_.size());

  EXPECT_EQ(std::string(good0), loaded_[0]->id());
  EXPECT_EQ(std::string("My extension 1"),
            loaded_[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_[0]->description());
  EXPECT_EQ(Manifest::INTERNAL, loaded_[0]->location());
  EXPECT_TRUE(service()->GetExtensionById(loaded_[0]->id(), false));
  EXPECT_EQ(expected_num_extensions, registry()->enabled_extensions().size());

  ValidatePrefKeyCount(3);
  ValidateIntegerPref(good0, "state", Extension::ENABLED);
  ValidateIntegerPref(good0, "location", Manifest::INTERNAL);
  ValidateIntegerPref(good1, "state", Extension::ENABLED);
  ValidateIntegerPref(good1, "location", Manifest::INTERNAL);
  ValidateIntegerPref(good2, "state", Extension::ENABLED);
  ValidateIntegerPref(good2, "location", Manifest::INTERNAL);

  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "file:///*");
  AddPattern(&expected_patterns, "http://*.google.com/*");
  AddPattern(&expected_patterns, "https://*.google.com/*");
  const Extension* extension = loaded_[0].get();
  const extensions::UserScriptList& scripts =
      extensions::ContentScriptsInfo::GetContentScripts(extension);
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(expected_patterns, scripts[0].url_patterns());
  EXPECT_EQ(2u, scripts[0].js_scripts().size());
  ExtensionResource resource00(extension->id(),
                               scripts[0].js_scripts()[0].extension_root(),
                               scripts[0].js_scripts()[0].relative_path());
  base::FilePath expected_path =
      base::MakeAbsoluteFilePath(extension->path().AppendASCII("script1.js"));
  EXPECT_TRUE(resource00.ComparePathWithDefault(expected_path));
  ExtensionResource resource01(extension->id(),
                               scripts[0].js_scripts()[1].extension_root(),
                               scripts[0].js_scripts()[1].relative_path());
  expected_path =
      base::MakeAbsoluteFilePath(extension->path().AppendASCII("script2.js"));
  EXPECT_TRUE(resource01.ComparePathWithDefault(expected_path));
  EXPECT_TRUE(!extensions::PluginInfo::HasPlugins(extension));
  EXPECT_EQ(1u, scripts[1].url_patterns().patterns().size());
  EXPECT_EQ("http://*.news.com/*",
            scripts[1].url_patterns().begin()->GetAsString());
  ExtensionResource resource10(extension->id(),
                               scripts[1].js_scripts()[0].extension_root(),
                               scripts[1].js_scripts()[0].relative_path());
  expected_path =
      extension->path().AppendASCII("js_files").AppendASCII("script3.js");
  expected_path = base::MakeAbsoluteFilePath(expected_path);
  EXPECT_TRUE(resource10.ComparePathWithDefault(expected_path));

  expected_patterns.ClearPatterns();
  AddPattern(&expected_patterns, "http://*.google.com/*");
  AddPattern(&expected_patterns, "https://*.google.com/*");
  EXPECT_EQ(
      expected_patterns,
      extension->permissions_data()->active_permissions().explicit_hosts());

  EXPECT_EQ(std::string(good1), loaded_[1]->id());
  EXPECT_EQ(std::string("My extension 2"), loaded_[1]->name());
  EXPECT_EQ(std::string(), loaded_[1]->description());
  EXPECT_EQ(loaded_[1]->GetResourceURL("background.html"),
            extensions::BackgroundInfo::GetBackgroundURL(loaded_[1].get()));
  EXPECT_EQ(0u,
            extensions::ContentScriptsInfo::GetContentScripts(loaded_[1].get())
                .size());

  // We don't parse the plugins section on Chrome OS.
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(!extensions::PluginInfo::HasPlugins(loaded_[1].get()));
#else
  ASSERT_TRUE(extensions::PluginInfo::HasPlugins(loaded_[1].get()));
  const std::vector<extensions::PluginInfo>* plugins =
      extensions::PluginInfo::GetPlugins(loaded_[1].get());
  ASSERT_TRUE(plugins);
  ASSERT_EQ(2u, plugins->size());
  EXPECT_EQ(loaded_[1]->path().AppendASCII("content_plugin.dll").value(),
            plugins->at(0).path.value());
  EXPECT_TRUE(plugins->at(0).is_public);
  EXPECT_EQ(loaded_[1]->path().AppendASCII("extension_plugin.dll").value(),
            plugins->at(1).path.value());
  EXPECT_FALSE(plugins->at(1).is_public);
#endif

  EXPECT_EQ(Manifest::INTERNAL, loaded_[1]->location());

  int index = expected_num_extensions - 1;
  EXPECT_EQ(std::string(good2), loaded_[index]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_[index]->name());
  EXPECT_EQ(std::string(), loaded_[index]->description());
  EXPECT_EQ(0u,
            extensions::ContentScriptsInfo::GetContentScripts(
                loaded_[index].get()).size());
  EXPECT_EQ(Manifest::INTERNAL, loaded_[index]->location());
}

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectoryFail) {
  // Initialize the test dir with a bad Preferences/extensions.
  base::FilePath source_install_dir =
      data_dir().AppendASCII("bad").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service()->Init();

  ASSERT_EQ(4u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());

  EXPECT_TRUE(base::MatchPattern(base::UTF16ToUTF8(GetErrors()[0]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
      extensions::manifest_errors::kManifestUnreadable)) <<
      base::UTF16ToUTF8(GetErrors()[0]);

  EXPECT_TRUE(base::MatchPattern(base::UTF16ToUTF8(GetErrors()[1]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
      extensions::manifest_errors::kManifestUnreadable)) <<
      base::UTF16ToUTF8(GetErrors()[1]);

  EXPECT_TRUE(base::MatchPattern(base::UTF16ToUTF8(GetErrors()[2]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
      extensions::manifest_errors::kMissingFile)) <<
      base::UTF16ToUTF8(GetErrors()[2]);

  EXPECT_TRUE(base::MatchPattern(base::UTF16ToUTF8(GetErrors()[3]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
      extensions::manifest_errors::kManifestUnreadable)) <<
      base::UTF16ToUTF8(GetErrors()[3]);
}

// Test various cases for delayed install because of missing imports.
TEST_F(ExtensionServiceTest, PendingImports) {
  InitPluginService();

  base::FilePath source_install_dir =
      data_dir().AppendASCII("pending_updates_with_imports").AppendASCII(
          "Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // Verify there are no pending extensions initially.
  EXPECT_FALSE(service()->pending_extension_manager()->HasPendingExtensions());

  service()->Init();
  // Wait for GarbageCollectExtensions task to complete.
  base::RunLoop().RunUntilIdle();

  // These extensions are used by the extensions we test below, they must be
  // installed.
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/1.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/2")));

  // Each of these extensions should have been rejected because of dependencies
  // that cannot be satisfied.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("cccccccccccccccccccccccccccccccc"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("cccccccccccccccccccccccccccccccc"));

  // Make sure the import started for the extension with a dependency.
  EXPECT_TRUE(
      prefs->GetDelayedInstallInfo("behllobkkfkfnphdnhnkndlbkcpglgmj"));
  EXPECT_EQ(ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
      prefs->GetDelayedInstallReason("behllobkkfkfnphdnhnkndlbkcpglgmj"));

  EXPECT_FALSE(base::PathExists(extensions_install_dir().AppendASCII(
      "behllobkkfkfnphdnhnkndlbkcpglgmj/1.0.0.0")));

  EXPECT_TRUE(service()->pending_extension_manager()->HasPendingExtensions());
  std::string pending_id("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(pending_id));
  // Remove it because we are not testing the pending extension manager's
  // ability to download and install extensions.
  EXPECT_TRUE(service()->pending_extension_manager()->Remove(pending_id));
}

// Test installing extensions. This test tries to install few extensions using
// crx files. If you need to change those crx files, feel free to repackage
// them, throw away the key used and change the id's above.
TEST_F(ExtensionServiceTest, InstallExtension) {
  InitializeEmptyExtensionService();

  // Extensions not enabled.
  service()->set_extensions_enabled(false);
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_FAILED);
  service()->set_extensions_enabled(true);

  ValidatePrefKeyCount(0);

  // A simple extension that should install without error.
  path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  // TODO(erikkay): verify the contents of the installed extension.

  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::INTERNAL);

  // An extension with page actions.
  path = data_dir().AppendASCII("page_action.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(page_action, "state", Extension::ENABLED);
  ValidateIntegerPref(page_action, "location", Manifest::INTERNAL);

  // Bad signature.
  path = data_dir().AppendASCII("bad_signature.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // 0-length extension file.
  path = data_dir().AppendASCII("not_an_extension.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // Bad magic number.
  path = data_dir().AppendASCII("bad_magic.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // Packed extensions may have folders or files that have underscores.
  // This will only cause a warning, rather than a fatal error.
  path = data_dir().AppendASCII("bad_underscore.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);

  // A test for an extension with a 2048-bit public key.
  path = data_dir().AppendASCII("good2048.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(good2048, "state", Extension::ENABLED);
  ValidateIntegerPref(good2048, "location", Manifest::INTERNAL);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

struct MockExtensionRegistryObserver
    : public extensions::ExtensionRegistryObserver {
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    last_extension_installed = extension->id();
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override {
    last_extension_uninstalled = extension->id();
  }

  std::string last_extension_installed;
  std::string last_extension_uninstalled;
};

// Test that correct notifications are sent to ExtensionRegistryObserver on
// extension install and uninstall.
TEST_F(ExtensionServiceTest, InstallObserverNotified) {
  InitializeEmptyExtensionService();

  extensions::ExtensionRegistry* registry(
      extensions::ExtensionRegistry::Get(profile()));
  MockExtensionRegistryObserver observer;
  registry->AddObserver(&observer);

  // A simple extension that should install without error.
  ASSERT_TRUE(observer.last_extension_installed.empty());
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, observer.last_extension_installed);

  // Uninstall the extension.
  ASSERT_TRUE(observer.last_extension_uninstalled.empty());
  UninstallExtension(good_crx, false);
  ASSERT_EQ(good_crx, observer.last_extension_uninstalled);

  registry->RemoveObserver(&observer);
}

// Tests that flags passed to OnExternalExtensionFileFound() make it to the
// extension object.
TEST_F(ExtensionServiceTest, InstallingExternalExtensionWithFlags) {
  const char kPrefFromBookmark[] = "from_bookmark";

  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  service()->set_extensions_enabled(true);

  // Register and install an external extension.
  scoped_ptr<Version> version(new Version("1.0.0.0"));
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      good_crx, std::move(version), path, Manifest::EXTERNAL_PREF,
      Extension::FROM_BOOKMARK, false /* mark_acknowledged */,
      false /* install_immediately */));
  if (service()->OnExternalExtensionFileFound(*info))
    observer.Wait();

  const Extension* extension = service()->GetExtensionById(good_crx, false);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension->from_bookmark());
  ASSERT_TRUE(ValidateBooleanPref(good_crx, kPrefFromBookmark, true));

  // Upgrade to version 2.0, the flag should be preserved.
  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, kPrefFromBookmark, true));
  extension = service()->GetExtensionById(good_crx, false);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension->from_bookmark());
}

// Test the handling of Extension::EXTERNAL_EXTENSION_UNINSTALLED
TEST_F(ExtensionServiceTest, UninstallingExternalExtensions) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  service()->set_extensions_enabled(true);

  // Install an external extension.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  scoped_ptr<Version> version(new Version("1.0.0.0"));
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      good_crx, std::move(version), path, Manifest::EXTERNAL_PREF,
      Extension::NO_FLAGS, false, false));
  if (service()->OnExternalExtensionFileFound(*info))
    observer.Wait();

  ASSERT_TRUE(service()->GetExtensionById(good_crx, false));

  // Uninstall it and check that its killbit gets set.
  UninstallExtension(good_crx, false);
  ValidateIntegerPref(good_crx, "state",
                      Extension::EXTERNAL_EXTENSION_UNINSTALLED);

  // Try to re-install it externally. This should fail because of the killbit.
  service()->OnExternalExtensionFileFound(*info);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(NULL == service()->GetExtensionById(good_crx, false));
  ValidateIntegerPref(good_crx, "state",
                      Extension::EXTERNAL_EXTENSION_UNINSTALLED);

  version.reset(new Version("1.0.0.1"));
  // Repeat the same thing with a newer version of the extension.
  path = data_dir().AppendASCII("good2.crx");
  info.reset(new ExternalInstallInfoFile(good_crx, std::move(version), path,
                                         Manifest::EXTERNAL_PREF,
                                         Extension::NO_FLAGS, false, false));
  service()->OnExternalExtensionFileFound(*info);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(NULL == service()->GetExtensionById(good_crx, false));
  ValidateIntegerPref(good_crx, "state",
                      Extension::EXTERNAL_EXTENSION_UNINSTALLED);

  // Try adding the same extension from an external update URL.
  ASSERT_FALSE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      good_crx,
      std::string(),
      GURL("http:://fake.update/url"),
      Manifest::EXTERNAL_PREF_DOWNLOAD,
      Extension::NO_FLAGS,
      false));

  ASSERT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));
}

// Test that uninstalling an external extension does not crash when
// the extension could not be loaded.
// This extension shown in preferences file requires an experimental permission.
// It could not be loaded without such permission.
TEST_F(ExtensionServiceTest, UninstallingNotLoadedExtension) {
  base::FilePath source_install_dir =
      data_dir().AppendASCII("good").AppendASCII("Extensions");
  // The preference contains an external extension
  // that requires 'experimental' permission.
  base::FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("PreferencesExperimental");

  // Aforementioned extension will not be loaded if
  // there is no '--enable-experimental-extension-apis' command line flag.
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service()->Init();

  // Check and try to uninstall it.
  // If we don't check whether the extension is loaded before we uninstall it
  // in CheckExternalUninstall, a crash will happen here because we will get or
  // dereference a NULL pointer (extension) inside UninstallExtension.
  MockExtensionProvider provider(NULL, Manifest::EXTERNAL_REGISTRY);
  service()->OnExternalProviderReady(&provider);
}

// Test that external extensions with incorrect IDs are not installed.
TEST_F(ExtensionServiceTest, FailOnWrongId) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  service()->set_extensions_enabled(true);

  scoped_ptr<Version> version(new Version("1.0.0.0"));

  const std::string wrong_id = all_zero;
  const std::string correct_id = good_crx;
  ASSERT_NE(correct_id, wrong_id);

  // Install an external extension with an ID from the external
  // source that is not equal to the ID in the extension manifest.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      wrong_id, std::move(version), path, Manifest::EXTERNAL_PREF,
      Extension::NO_FLAGS, false, false));
  service()->OnExternalExtensionFileFound(*info);

  observer.Wait();
  ASSERT_FALSE(service()->GetExtensionById(good_crx, false));

  // Try again with the right ID. Expect success.
  content::WindowedNotificationObserver observer2(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  info->extension_id = correct_id;
  if (service()->OnExternalExtensionFileFound(*info))
    observer2.Wait();
  ASSERT_TRUE(service()->GetExtensionById(good_crx, false));
}

// Test that external extensions with incorrect versions are not installed.
TEST_F(ExtensionServiceTest, FailOnWrongVersion) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  service()->set_extensions_enabled(true);

  // Install an external extension with a version from the external
  // source that is not equal to the version in the extension manifest.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  scoped_ptr<Version> wrong_version(new Version("1.2.3.4"));
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      good_crx, std::move(wrong_version), path, Manifest::EXTERNAL_PREF,
      Extension::NO_FLAGS, false, false));
  service()->OnExternalExtensionFileFound(*info);

  observer.Wait();
  ASSERT_FALSE(service()->GetExtensionById(good_crx, false));

  // Try again with the right version. Expect success.
  service()->pending_extension_manager()->Remove(good_crx);
  scoped_ptr<Version> correct_version(new Version("1.0.0.0"));
  info->version = std::move(correct_version);
  content::WindowedNotificationObserver observer2(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  if (service()->OnExternalExtensionFileFound(*info))
    observer2.Wait();
  ASSERT_TRUE(service()->GetExtensionById(good_crx, false));
}

// Install a user script (they get converted automatically to an extension)
TEST_F(ExtensionServiceTest, InstallUserScript) {
  // The details of script conversion are tested elsewhere, this just tests
  // integration with ExtensionService.
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("user_script_basic.user.js");

  ASSERT_TRUE(base::PathExists(path));
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  installer->InstallUserScript(
      path,
      GURL("http://www.aaronboodman.com/scripts/user_script_basic.user.js"));

  base::RunLoop().RunUntilIdle();
  std::vector<base::string16> errors = GetErrors();
  EXPECT_TRUE(installed_) << "Nothing was installed.";
  EXPECT_FALSE(was_update_) << path.value();
  ASSERT_EQ(1u, loaded_.size()) << "Nothing was loaded.";
  EXPECT_EQ(0u, errors.size())
      << "There were errors: "
      << base::JoinString(errors, base::ASCIIToUTF16(","));
  EXPECT_TRUE(service()->GetExtensionById(loaded_[0]->id(), false))
      << path.value();

  installed_ = NULL;
  was_update_ = false;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();
}

// Extensions don't install during shutdown.
TEST_F(ExtensionServiceTest, InstallExtensionDuringShutdown) {
  InitializeEmptyExtensionService();

  // Simulate shutdown.
  service()->set_browser_terminating_for_test(true);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  installer->InstallCrx(path);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(installed_) << "Extension installed during shutdown.";
  ASSERT_EQ(0u, loaded_.size()) << "Extension loaded during shutdown.";
}

// This tests that the granted permissions preferences are correctly set when
// installing an extension.
TEST_F(ExtensionServiceTest, GrantedPermissions) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");

  base::FilePath pem_path = path.AppendASCII("unknown.pem");
  path = path.AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_perms;
  URLPatternSet expected_host_perms;

  // Make sure there aren't any granted permissions before the
  // extension is installed.
  EXPECT_FALSE(prefs->GetGrantedPermissions(permissions_crx).get());

  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(permissions_crx, extension->id());

  // Verify that the valid API permissions have been recognized.
  expected_api_perms.insert(APIPermission::kTab);

  AddPattern(&expected_host_perms, "http://*.google.com/*");
  AddPattern(&expected_host_perms, "https://*.google.com/*");
  AddPattern(&expected_host_perms, "http://*.google.com.hk/*");
  AddPattern(&expected_host_perms, "http://www.example.com/*");

  scoped_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension->id());
  EXPECT_TRUE(known_perms.get());
  EXPECT_FALSE(known_perms->IsEmpty());
  EXPECT_EQ(expected_api_perms, known_perms->apis());
  EXPECT_FALSE(known_perms->HasEffectiveFullAccess());
  EXPECT_EQ(expected_host_perms, known_perms->effective_hosts());
}


#if !defined(OS_CHROMEOS)
// This tests that the granted permissions preferences are correctly set for
// default apps.
TEST_F(ExtensionServiceTest, DefaultAppsGrantedPermissions) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");

  base::FilePath pem_path = path.AppendASCII("unknown.pem");
  path = path.AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_perms;
  URLPatternSet expected_host_perms;

  // Make sure there aren't any granted permissions before the
  // extension is installed.
  EXPECT_FALSE(prefs->GetGrantedPermissions(permissions_crx).get());

  const Extension* extension = PackAndInstallCRX(
      path, pem_path, INSTALL_NEW, Extension::WAS_INSTALLED_BY_DEFAULT);

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(permissions_crx, extension->id());

  // Verify that the valid API permissions have been recognized.
  expected_api_perms.insert(APIPermission::kTab);

  scoped_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension->id());
  EXPECT_TRUE(known_perms.get());
  EXPECT_FALSE(known_perms->IsEmpty());
  EXPECT_EQ(expected_api_perms, known_perms->apis());
  EXPECT_FALSE(known_perms->HasEffectiveFullAccess());
}
#endif

#if !defined(OS_POSIX) || defined(OS_MACOSX)
// Tests that the granted permissions full_access bit gets set correctly when
// an extension contains an NPAPI plugin.
// Only run this on platforms that support NPAPI plugins.
TEST_F(ExtensionServiceTest, GrantedFullAccessPermissions) {
  InitPluginService();

  InitializeEmptyExtensionService();

  ASSERT_TRUE(base::PathExists(good1_path()));
  const Extension* extension = PackAndInstallCRX(good1_path(), INSTALL_NEW);
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  scoped_ptr<const PermissionSet> permissions =
      prefs->GetGrantedPermissions(extension->id());
  EXPECT_FALSE(permissions->IsEmpty());
  EXPECT_TRUE(permissions->HasEffectiveFullAccess());
  EXPECT_FALSE(permissions->apis().empty());
  EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kPlugin));

  // Full access implies full host access too...
  EXPECT_TRUE(permissions->HasEffectiveAccessToAllHosts());
}
#endif

// Tests that the extension is disabled when permissions are missing from
// the extension's granted permissions preferences. (This simulates updating
// the browser to a version which recognizes more permissions).
TEST_F(ExtensionServiceTest, GrantedAPIAndHostPermissions) {
  InitializeEmptyExtensionService();

  base::FilePath path =
      data_dir().AppendASCII("permissions").AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(path));

  const Extension* extension = PackAndInstallCRX(path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  std::string extension_id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_permissions;
  URLPatternSet expected_host_permissions;

  expected_api_permissions.insert(APIPermission::kTab);
  AddPattern(&expected_host_permissions, "http://*.google.com/*");
  AddPattern(&expected_host_permissions, "https://*.google.com/*");
  AddPattern(&expected_host_permissions, "http://*.google.com.hk/*");
  AddPattern(&expected_host_permissions, "http://www.example.com/*");

  std::set<std::string> host_permissions;

  // Test that the extension is disabled when an API permission is missing from
  // the extension's granted api permissions preference. (This simulates
  // updating the browser to a version which recognizes a new API permission).
  SetPref(extension_id, "granted_permissions.api",
          new base::ListValue(), "granted_permissions.api");
  service()->ReloadExtensionsForTest();

  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  extension = registry()->disabled_extensions().begin()->get();

  ASSERT_TRUE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_FALSE(service()->IsExtensionEnabled(extension_id));
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service()->GrantPermissionsAndEnableExtension(extension);

  ASSERT_FALSE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_TRUE(service()->IsExtensionEnabled(extension_id));
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  scoped_ptr<const PermissionSet> current_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(current_perms.get());
  ASSERT_FALSE(current_perms->IsEmpty());
  ASSERT_FALSE(current_perms->HasEffectiveFullAccess());
  ASSERT_EQ(expected_api_permissions, current_perms->apis());
  ASSERT_EQ(expected_host_permissions, current_perms->effective_hosts());

  // Tests that the extension is disabled when a host permission is missing from
  // the extension's granted host permissions preference. (This simulates
  // updating the browser to a version which recognizes additional host
  // permissions).
  host_permissions.clear();
  current_perms = NULL;

  host_permissions.insert("http://*.google.com/*");
  host_permissions.insert("https://*.google.com/*");
  host_permissions.insert("http://*.google.com.hk/*");

  base::ListValue* api_permissions = new base::ListValue();
  api_permissions->Append(
      new base::StringValue("tabs"));
  SetPref(extension_id, "granted_permissions.api",
          api_permissions, "granted_permissions.api");
  SetPrefStringSet(
      extension_id, "granted_permissions.scriptable_host", host_permissions);

  service()->ReloadExtensionsForTest();

  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  extension = registry()->disabled_extensions().begin()->get();

  ASSERT_TRUE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_FALSE(service()->IsExtensionEnabled(extension_id));
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service()->GrantPermissionsAndEnableExtension(extension);

  ASSERT_TRUE(service()->IsExtensionEnabled(extension_id));
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  current_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(current_perms.get());
  ASSERT_FALSE(current_perms->IsEmpty());
  ASSERT_FALSE(current_perms->HasEffectiveFullAccess());
  ASSERT_EQ(expected_api_permissions, current_perms->apis());
  ASSERT_EQ(expected_host_permissions, current_perms->effective_hosts());
}

// Test Packaging and installing an extension.
TEST_F(ExtensionServiceTest, PackExtension) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory =
      data_dir()
          .AppendASCII("good")
          .AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = temp_dir.path();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  base::FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));
  ASSERT_TRUE(base::PathExists(crx_path));
  ASSERT_TRUE(base::PathExists(privkey_path));

  // Repeat the run with the pem file gone, and no special flags
  // Should refuse to overwrite the existing crx.
  base::DeleteFile(privkey_path, false);
  ASSERT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));

  // OK, now try it with a flag to overwrite existing crx.  Should work.
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));

  // Repeat the run allowing existing crx, but the existing pem is still
  // an error.  Should fail.
  ASSERT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));

  ASSERT_TRUE(base::PathExists(privkey_path));
  InstallCRX(crx_path, INSTALL_NEW);

  // Try packing with invalid paths.
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(
      creator->Run(base::FilePath(), base::FilePath(), base::FilePath(),
                   base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing an empty directory. Should fail because an empty directory is
  // not a valid extension.
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(creator->Run(temp_dir2.path(), crx_path, privkey_path,
                            base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing with an invalid manifest.
  std::string invalid_manifest_content = "I am not a manifest.";
  ASSERT_TRUE(base::WriteFile(
      temp_dir2.path().Append(extensions::kManifestFilename),
      invalid_manifest_content.c_str(), invalid_manifest_content.size()));
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(creator->Run(temp_dir2.path(), crx_path, privkey_path,
                            base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing with a private key that is a valid key, but invalid for the
  // extension.
  base::FilePath bad_private_key_dir =
      data_dir().AppendASCII("bad_private_key");
  crx_path = output_directory.AppendASCII("bad_private_key.crx");
  privkey_path = data_dir().AppendASCII("bad_private_key.pem");
  ASSERT_FALSE(creator->Run(bad_private_key_dir, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));
}

// Test Packaging and installing an extension whose name contains punctuation.
TEST_F(ExtensionServiceTest, PackPunctuatedExtension) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory = data_dir()
                                       .AppendASCII("good")
                                       .AppendASCII("Extensions")
                                       .AppendASCII(good0)
                                       .AppendASCII("1.0.0.0");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Extension names containing punctuation, and the expected names for the
  // packed extensions.
  const base::FilePath punctuated_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods")),
    base::FilePath(FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname/")).
        NormalizePathSeparators(),
  };
  const base::FilePath expected_crx_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods.crx")),
    base::FilePath(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.crx")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname.crx")),
  };
  const base::FilePath expected_private_key_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods.pem")),
    base::FilePath(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.pem")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname.pem")),
  };

  for (size_t i = 0; i < arraysize(punctuated_names); ++i) {
    SCOPED_TRACE(punctuated_names[i].value().c_str());
    base::FilePath output_dir = temp_dir.path().Append(punctuated_names[i]);

    // Copy the extension into the output directory, as PackExtensionJob doesn't
    // let us choose where to output the packed extension.
    ASSERT_TRUE(base::CopyDirectory(input_directory, output_dir, true));

    base::FilePath expected_crx_path =
        temp_dir.path().Append(expected_crx_names[i]);
    base::FilePath expected_private_key_path =
        temp_dir.path().Append(expected_private_key_names[i]);
    PackExtensionTestClient pack_client(expected_crx_path,
                                        expected_private_key_path);
    scoped_refptr<extensions::PackExtensionJob> packer(
        new extensions::PackExtensionJob(&pack_client, output_dir,
                                         base::FilePath(),
                                         ExtensionCreator::kOverwriteCRX));
    packer->Start();

    // The packer will post a notification task to the current thread's message
    // loop when it is finished.  We manually run the loop here so that we
    // block and catch the notification; otherwise, the process would exit.
    // This call to |Run()| is matched by a call to |Quit()| in the
    // |PackExtensionTestClient|'s notification handling code.
    base::MessageLoop::current()->Run();

    if (HasFatalFailure())
      return;

    InstallCRX(expected_crx_path, INSTALL_NEW);
  }
}

TEST_F(ExtensionServiceTest, PackExtensionContainingKeyFails) {
  InitializeEmptyExtensionService();

  base::ScopedTempDir extension_temp_dir;
  ASSERT_TRUE(extension_temp_dir.CreateUniqueTempDir());
  base::FilePath input_directory = extension_temp_dir.path().AppendASCII("ext");
  ASSERT_TRUE(
      base::CopyDirectory(data_dir()
                              .AppendASCII("good")
                              .AppendASCII("Extensions")
                              .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                              .AppendASCII("1.0.0.0"),
                          input_directory,
                          /*recursive=*/true));

  base::ScopedTempDir output_temp_dir;
  ASSERT_TRUE(output_temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = output_temp_dir.path();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  base::FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  // Pack the extension once to get a private key.
  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags))
      << creator->error_message();
  ASSERT_TRUE(base::PathExists(crx_path));
  ASSERT_TRUE(base::PathExists(privkey_path));

  base::DeleteFile(crx_path, false);
  // Move the pem file into the extension.
  base::Move(privkey_path,
                  input_directory.AppendASCII("privkey.pem"));

  // This pack should fail because of the contained private key.
  EXPECT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));
  EXPECT_THAT(creator->error_message(),
              testing::ContainsRegex(
                  "extension includes the key file.*privkey.pem"));
}

// Test Packaging and installing an extension using an openssl generated key.
// The openssl is generated with the following:
// > openssl genrsa -out privkey.pem 1024
// > openssl pkcs8 -topk8 -nocrypt -in privkey.pem -out privkey_asn1.pem
// The privkey.pem is a PrivateKey, and the pcks8 -topk8 creates a
// PrivateKeyInfo ASN.1 structure, we our RSAPrivateKey expects.
TEST_F(ExtensionServiceTest, PackExtensionOpenSSLKey) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory =
      data_dir()
          .AppendASCII("good")
          .AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0");
  base::FilePath privkey_path(
      data_dir().AppendASCII("openssl_privkey_asn1.pem"));
  ASSERT_TRUE(base::PathExists(privkey_path));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = temp_dir.path();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, privkey_path,
      base::FilePath(), ExtensionCreator::kOverwriteCRX));

  InstallCRX(crx_path, INSTALL_NEW);
}

#if defined(THREAD_SANITIZER)
// Flaky under Tsan. http://crbug.com/377702
#define MAYBE_InstallTheme DISABLED_InstallTheme
#else
#define MAYBE_InstallTheme InstallTheme
#endif

TEST_F(ExtensionServiceTest, MAYBE_InstallTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  // A theme.
  base::FilePath path = data_dir().AppendASCII("theme.crx");
  InstallCRX(path, INSTALL_NEW);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme_crx, "location", Manifest::INTERNAL);

  // A theme when extensions are disabled. Themes can be installed, even when
  // extensions are disabled.
  service()->set_extensions_enabled(false);
  path = data_dir().AppendASCII("theme2.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme2_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme2_crx, "location", Manifest::INTERNAL);

  // A theme with extension elements. Themes cannot have extension elements,
  // so any such elements (like content scripts) should be ignored.
  service()->set_extensions_enabled(true);
  {
    path = data_dir().AppendASCII("theme_with_extension.crx");
    const Extension* extension = InstallCRX(path, INSTALL_NEW);
    ValidatePrefKeyCount(++pref_count);
    ASSERT_TRUE(extension);
    EXPECT_TRUE(extension->is_theme());
    EXPECT_EQ(
        0u,
        extensions::ContentScriptsInfo::GetContentScripts(extension).size());
  }

  // A theme with image resources missing (misspelt path).
  path = data_dir().AppendASCII("theme_missing_image.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);
}

TEST_F(ExtensionServiceTest, LoadLocalizedTheme) {
  // Load.
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath extension_path = data_dir().AppendASCII("theme_i18n");

  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  const Extension* theme = registry()->enabled_extensions().begin()->get();
  EXPECT_EQ("name", theme->name());
  EXPECT_EQ("description", theme->description());

  // Cleanup the "Cached Theme.pak" file (or "Cached Theme Material Design.pak"
  // when Material Design is enabled). Ideally, this would be installed in a
  // temporary directory, but it automatically installs to the extension's
  // directory, and we don't want to copy the whole extension for a unittest.
  base::FilePath theme_file = extension_path.Append(
      ui::MaterialDesignController::IsModeMaterial()
           ? chrome::kThemePackMaterialDesignFilename
           : chrome::kThemePackFilename);
  ASSERT_TRUE(base::PathExists(theme_file));
  ASSERT_TRUE(base::DeleteFile(theme_file, false));  // Not recursive.
}

#if defined(OS_POSIX)
TEST_F(ExtensionServiceTest, UnpackedExtensionMayContainSymlinkedFiles) {
  base::FilePath source_data_dir =
      data_dir().AppendASCII("unpacked").AppendASCII("symlinks_allowed");

  // Paths to test data files.
  base::FilePath source_manifest = source_data_dir.AppendASCII("manifest.json");
  ASSERT_TRUE(base::PathExists(source_manifest));
  base::FilePath source_icon = source_data_dir.AppendASCII("icon.png");
  ASSERT_TRUE(base::PathExists(source_icon));

  // Set up the temporary extension directory.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath extension_path = temp.path();
  base::FilePath manifest = extension_path.Append(
      extensions::kManifestFilename);
  base::FilePath icon_symlink = extension_path.AppendASCII("icon.png");
  base::CopyFile(source_manifest, manifest);
  base::CreateSymbolicLink(source_icon, icon_symlink);

  // Load extension.
  InitializeEmptyExtensionService();
  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetErrors().empty());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
}
#endif

TEST_F(ExtensionServiceTest, UnpackedExtensionMayNotHaveUnderscore) {
  InitializeEmptyExtensionService();
  base::FilePath extension_path = data_dir().AppendASCII("underscore_name");
  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

TEST_F(ExtensionServiceTest, InstallLocalizedTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath theme_path = data_dir().AppendASCII("theme_i18n");

  const Extension* theme = PackAndInstallCRX(theme_path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("name", theme->name());
  EXPECT_EQ("description", theme->description());
}

TEST_F(ExtensionServiceTest, InstallApps) {
  InitializeEmptyExtensionService();

  // An empty app.
  const Extension* app =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  ValidateIntegerPref(app->id(), "state", Extension::ENABLED);
  ValidateIntegerPref(app->id(), "location", Manifest::INTERNAL);

  // Another app with non-overlapping extent. Should succeed.
  PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);

  // A third app whose extent overlaps the first. Should fail.
  PackAndInstallCRX(data_dir().AppendASCII("app3"), INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);
}

// Tests that file access is OFF by default.
TEST_F(ExtensionServiceTest, DefaultFileAccess) {
  InitializeEmptyExtensionService();
  const Extension* extension = PackAndInstallCRX(
      data_dir().AppendASCII("permissions").AppendASCII("files"), INSTALL_NEW);
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_FALSE(
      ExtensionPrefs::Get(profile())->AllowFileAccess(extension->id()));
}

TEST_F(ExtensionServiceTest, UpdateApps) {
  InitializeEmptyExtensionService();
  base::FilePath extensions_path = data_dir().AppendASCII("app_update");

  // First install v1 of a hosted app.
  const Extension* extension =
      InstallCRX(extensions_path.AppendASCII("v1.crx"), INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  std::string id = extension->id();
  ASSERT_EQ(std::string("1"), extension->version()->GetString());

  // Now try updating to v2.
  UpdateExtension(id,
                  extensions_path.AppendASCII("v2.crx"),
                  ENABLED);
  ASSERT_EQ(std::string("2"),
            service()->GetExtensionById(id, false)->version()->GetString());
}

// Verifies that the NTP page and launch ordinals are kept when updating apps.
TEST_F(ExtensionServiceTest, UpdateAppsRetainOrdinals) {
  InitializeEmptyExtensionService();
  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  base::FilePath extensions_path = data_dir().AppendASCII("app_update");

  // First install v1 of a hosted app.
  const Extension* extension =
      InstallCRX(extensions_path.AppendASCII("v1.crx"), INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  std::string id = extension->id();
  ASSERT_EQ(std::string("1"), extension->version()->GetString());

  // Modify the ordinals so we can distinguish them from the defaults.
  syncer::StringOrdinal new_page_ordinal =
      sorting->GetPageOrdinal(id).CreateAfter();
  syncer::StringOrdinal new_launch_ordinal =
      sorting->GetAppLaunchOrdinal(id).CreateBefore();

  sorting->SetPageOrdinal(id, new_page_ordinal);
  sorting->SetAppLaunchOrdinal(id, new_launch_ordinal);

  // Now try updating to v2.
  UpdateExtension(id, extensions_path.AppendASCII("v2.crx"), ENABLED);
  ASSERT_EQ(std::string("2"),
            service()->GetExtensionById(id, false)->version()->GetString());

  // Verify that the ordinals match.
  ASSERT_TRUE(new_page_ordinal.Equals(sorting->GetPageOrdinal(id)));
  ASSERT_TRUE(new_launch_ordinal.Equals(sorting->GetAppLaunchOrdinal(id)));
}

// Ensures that the CWS has properly initialized ordinals.
TEST_F(ExtensionServiceTest, EnsureCWSOrdinalsInitialized) {
  InitializeEmptyExtensionService();
  service()->component_loader()->Add(
      IDR_WEBSTORE_MANIFEST, base::FilePath(FILE_PATH_LITERAL("web_store")));
  service()->Init();

  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  EXPECT_TRUE(
      sorting->GetPageOrdinal(extensions::kWebStoreAppId).IsValid());
  EXPECT_TRUE(
      sorting->GetAppLaunchOrdinal(extensions::kWebStoreAppId).IsValid());
}

TEST_F(ExtensionServiceTest, InstallAppsWithUnlimitedStorage) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(registry()->enabled_extensions().is_empty());

  int pref_count = 0;

  // Install app1 with unlimited storage.
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  const std::string id1 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin1(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  // Install app2 from the same origin with unlimited storage.
  extension = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin2(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  EXPECT_EQ(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin2));

  // Uninstall one of them, unlimited storage should still be granted
  // to the origin.
  UninstallExtension(id1, false);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  // Uninstall the other, unlimited storage should be revoked.
  UninstallExtension(id2, false);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          origin2));
}

TEST_F(ExtensionServiceTest, InstallAppsAndCheckStorageProtection) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(registry()->enabled_extensions().is_empty());

  int pref_count = 0;

  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->is_app());
  const std::string id1 = extension->id();
  const GURL origin1(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
      origin1));

  // App 4 has a different origin (maps.google.com).
  extension = PackAndInstallCRX(data_dir().AppendASCII("app4"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  const GURL origin2(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  ASSERT_NE(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
      origin2));

  UninstallExtension(id1, false);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(id2, false);

  EXPECT_TRUE(registry()->enabled_extensions().is_empty());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
          origin1));
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
          origin2));
}

// Test that when an extension version is reinstalled, nothing happens.
TEST_F(ExtensionServiceTest, Reinstall) {
  InitializeEmptyExtensionService();

  // A simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::INTERNAL);

  // Reinstall the same version, it should overwrite the previous one.
  InstallCRX(path, INSTALL_UPDATED);

  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::INTERNAL);
}

// Test that we can determine if extensions came from the
// Chrome web store.
TEST_F(ExtensionServiceTest, FromWebStore) {
  InitializeEmptyExtensionService();

  // A simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  // Not from web store.
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", false));
  ASSERT_FALSE(extension->from_webstore());

  // Test install from web store.
  InstallCRXFromWebStore(path, INSTALL_UPDATED);  // From web store.

  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", true));

  // Reload so extension gets reinitialized with new value.
  service()->ReloadExtensionsForTest();
  extension = service()->GetExtensionById(id, false);
  ASSERT_TRUE(extension->from_webstore());

  // Upgrade to version 2.0
  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", true));
}

// Test upgrading a signed extension.
TEST_F(ExtensionServiceTest, UpgradeSignedGood) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  ASSERT_EQ("1.0.0.0", extension->version()->GetString());
  ASSERT_EQ(0u, GetErrors().size());

  // Upgrade to version 1.0.0.1.
  // Also test that the extension's old and new title are correctly retrieved.
  path = data_dir().AppendASCII("good2.crx");
  InstallCRX(path, INSTALL_UPDATED, Extension::NO_FLAGS, "My extension 1");
  extension = service()->GetExtensionById(id, false);

  ASSERT_EQ("1.0.0.1", extension->version()->GetString());
  ASSERT_EQ("My updated extension 1", extension->name());
  ASSERT_EQ(0u, GetErrors().size());
}

// Test upgrading a signed extension with a bad signature.
TEST_F(ExtensionServiceTest, UpgradeSignedBad) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  // Try upgrading with a bad signature. This should fail during the unpack,
  // because the key will not match the signature.
  path = data_dir().AppendASCII("bad_signature.crx");
  InstallCRX(path, INSTALL_FAILED);
}

// Test a normal update via the UpdateExtension API
TEST_F(ExtensionServiceTest, UpdateExtension) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ASSERT_EQ(
      "1.0.0.1",
      service()->GetExtensionById(good_crx, false)->version()->GetString());
}

// Extensions should not be updated during browser shutdown.
TEST_F(ExtensionServiceTest, UpdateExtensionDuringShutdown) {
  InitializeEmptyExtensionService();

  // Install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, good->id());

  // Simulate shutdown.
  service()->set_browser_terminating_for_test(true);

  // Update should fail and extension should not be updated.
  path = data_dir().AppendASCII("good2.crx");
  bool updated = service()->UpdateExtension(
      extensions::CRXFileInfo(good_crx, path), true, NULL);
  ASSERT_FALSE(updated);
  ASSERT_EQ(
      "1.0.0.0",
      service()->GetExtensionById(good_crx, false)->version()->GetString());
}

// Test updating a not-already-installed extension - this should fail
TEST_F(ExtensionServiceTest, UpdateNotInstalledExtension) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(good_crx, path, UPDATED);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(0u, registry()->enabled_extensions().size());
  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
}

// Makes sure you can't downgrade an extension via UpdateExtension
TEST_F(ExtensionServiceTest, UpdateWillNotDowngrade) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good2.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.1", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Change path from good2.crx -> good.crx
  path = data_dir().AppendASCII("good.crx");
  UpdateExtension(good_crx, path, FAILED);
  ASSERT_EQ(
      "1.0.0.1",
      service()->GetExtensionById(good_crx, false)->version()->GetString());
}

// Make sure calling update with an identical version does nothing
TEST_F(ExtensionServiceTest, UpdateToSameVersionIsNoop) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);
}

// Tests that updating an extension does not clobber old state.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesState) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Disable it and allow it to run in incognito. These settings should carry
  // over to the updated version.
  service()->DisableExtension(good->id(), Extension::DISABLE_USER_ACTION);
  extensions::util::SetIsIncognitoEnabled(good->id(), profile(), true);

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, INSTALLED);
  ASSERT_EQ(1u, registry()->disabled_extensions().size());
  const Extension* good2 = service()->GetExtensionById(good_crx, true);
  ASSERT_EQ("1.0.0.1", good2->version()->GetString());
  EXPECT_TRUE(extensions::util::IsIncognitoEnabled(good2->id(), profile()));
  EXPECT_EQ(Extension::DISABLE_USER_ACTION,
            ExtensionPrefs::Get(profile())->GetDisableReasons(good2->id()));
}

// Tests that updating preserves extension location.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesLocation) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good =
      InstallCRXWithLocation(path, Manifest::EXTERNAL_PREF, INSTALL_NEW);

  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  const Extension* good2 = service()->GetExtensionById(good_crx, false);
  ASSERT_EQ("1.0.0.1", good2->version()->GetString());
  EXPECT_EQ(good2->location(), Manifest::EXTERNAL_PREF);
}

// Makes sure that LOAD extension types can downgrade.
TEST_F(ExtensionServiceTest, LoadExtensionsCanDowngrade) {
  InitializeEmptyExtensionService();

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // We'll write the extension manifest dynamically to a temporary path
  // to make it easier to change the version number.
  base::FilePath extension_path = temp.path();
  base::FilePath manifest_path =
      extension_path.Append(extensions::kManifestFilename);
  ASSERT_FALSE(base::PathExists(manifest_path));

  // Start with version 2.0.
  base::DictionaryValue manifest;
  manifest.SetString("version", "2.0");
  manifest.SetString("name", "LOAD Downgrade Test");
  manifest.SetInteger("manifest_version", 2);

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(manifest));

  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Manifest::UNPACKED, loaded_[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("2.0", loaded_[0]->VersionString());

  // Now set the version number to 1.0, reload the extensions and verify that
  // the downgrade was accepted.
  manifest.SetString("version", "1.0");
  ASSERT_TRUE(serializer.Serialize(manifest));

  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Manifest::UNPACKED, loaded_[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("1.0", loaded_[0]->VersionString());
}

#if !defined(OS_POSIX) || defined(OS_MACOSX)
// LOAD extensions with plugins require approval.
// Only run this on platforms that support NPAPI plugins.
TEST_F(ExtensionServiceTest, LoadExtensionsWithPlugins) {
  base::FilePath extension_with_plugin_path = good1_path();
  base::FilePath extension_no_plugin_path = good2_path();

  InitPluginService();
  InitializeEmptyExtensionService();
  service()->set_show_extensions_prompts(true);

  // Start by canceling any install prompts.
  scoped_ptr<extensions::ScopedTestDialogAutoConfirm> auto_confirm(
      new extensions::ScopedTestDialogAutoConfirm(
          extensions::ScopedTestDialogAutoConfirm::CANCEL));

  // The extension that has a plugin should not install.
  extensions::UnpackedInstaller::Create(service())
      ->Load(extension_with_plugin_path);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // But the extension with no plugin should since there's no prompt.
  ExtensionErrorReporter::GetInstance()->ClearErrors();
  extensions::UnpackedInstaller::Create(service())
      ->Load(extension_no_plugin_path);
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));

  // The plugin extension should install if we accept the dialog.
  auto_confirm.reset();
  auto_confirm.reset(new extensions::ScopedTestDialogAutoConfirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT));

  ExtensionErrorReporter::GetInstance()->ClearErrors();
  extensions::UnpackedInstaller::Create(service())
      ->Load(extension_with_plugin_path);
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(2u, loaded_.size());
  EXPECT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good1));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));

  // Make sure the granted permissions have been setup.
  scoped_ptr<const PermissionSet> permissions =
      ExtensionPrefs::Get(profile())->GetGrantedPermissions(good1);
  ASSERT_TRUE(permissions);
  EXPECT_FALSE(permissions->IsEmpty());
  EXPECT_TRUE(permissions->HasEffectiveFullAccess());
  EXPECT_FALSE(permissions->apis().empty());
  EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kPlugin));

  // We should be able to reload the extension without getting another prompt.
  loaded_.clear();
  auto_confirm.reset();
  auto_confirm.reset(new extensions::ScopedTestDialogAutoConfirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL));

  service()->ReloadExtension(good1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, loaded_.size());
  EXPECT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}
#endif  // !defined(OS_POSIX) || defined(OS_MACOSX)

namespace {

bool IsExtension(const Extension* extension) {
  return extension->GetType() == Manifest::TYPE_EXTENSION;
}

#if defined(ENABLE_BLACKLIST_TESTS)
std::set<std::string> StringSet(const std::string& s) {
  std::set<std::string> set;
  set.insert(s);
  return set;
}
std::set<std::string> StringSet(const std::string& s1, const std::string& s2) {
  std::set<std::string> set = StringSet(s1);
  set.insert(s2);
  return set;
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

}  // namespace

// Test adding a pending extension.
TEST_F(ExtensionServiceTest, AddPendingExtensionFromSync) {
  InitializeEmptyExtensionService();

  const std::string kFakeId(all_zero);
  const GURL kFakeUpdateURL("http:://fake.update/url");
  const bool kFakeRemoteInstall(false);
  const bool kFakeInstalledByCustodian(false);

  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kFakeId,
          kFakeUpdateURL,
          base::Version(),
          &IsExtension,
          kFakeRemoteInstall,
          kFakeInstalledByCustodian));

  const extensions::PendingExtensionInfo* pending_extension_info;
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kFakeId)));
  EXPECT_EQ(kFakeUpdateURL, pending_extension_info->update_url());
  EXPECT_EQ(&IsExtension, pending_extension_info->should_allow_install_);
  // Use
  // EXPECT_TRUE(kFakeRemoteInstall == pending_extension_info->remote_install())
  // instead of
  // EXPECT_EQ(kFakeRemoteInstall, pending_extension_info->remote_install())
  // as gcc 4.7 issues the following warning on EXPECT_EQ(false, x), which is
  // turned into an error with -Werror=conversion-null:
  //   converting 'false' to pointer type for argument 1 of
  //   'char testing::internal::IsNullLiteralHelper(testing::internal::Secret*)'
  // https://code.google.com/p/googletest/issues/detail?id=458
  EXPECT_TRUE(kFakeRemoteInstall == pending_extension_info->remote_install());
}

namespace {
const char kGoodId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodUpdateURL[] = "http://good.update/url";
const char kGoodVersion[] = "1";
const bool kGoodIsFromSync = true;
const bool kGoodRemoteInstall = false;
const bool kGoodInstalledByCustodian = false;
}  // namespace

// Test installing a pending extension (this goes through
// ExtensionService::UpdateExtension).
TEST_F(ExtensionServiceTest, UpdatePendingExtension) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(kGoodVersion),
          &IsExtension,
          kGoodRemoteInstall,
          kGoodInstalledByCustodian));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  const Extension* extension = service()->GetExtensionById(kGoodId, true);
  EXPECT_TRUE(extension);
}

TEST_F(ExtensionServiceTest, UpdatePendingExtensionWrongVersion) {
  InitializeEmptyExtensionService();
  base::Version other_version("0.1");
  ASSERT_TRUE(other_version.IsValid());
  ASSERT_NE(other_version, base::Version(kGoodVersion));
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          other_version,
          &IsExtension,
          kGoodRemoteInstall,
          kGoodInstalledByCustodian));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // After installation, the extension should be disabled, because it's missing
  // permissions.
  UpdateExtension(kGoodId, path, DISABLED);

  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->DidExtensionEscalatePermissions(kGoodId));

  // It should still have been installed though.
  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  const Extension* extension = service()->GetExtensionById(kGoodId, true);
  EXPECT_TRUE(extension);
}

namespace {

bool IsTheme(const Extension* extension) {
  return extension->is_theme();
}

}  // namespace

// Test updating a pending theme.
// Disabled due to ASAN failure. http://crbug.com/108320
TEST_F(ExtensionServiceTest, DISABLED_UpdatePendingTheme) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromSync(
      theme_crx, GURL(), base::Version(), &IsTheme, false, false));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension = service()->GetExtensionById(theme_crx, true);
  ASSERT_TRUE(extension);

  EXPECT_FALSE(
      ExtensionPrefs::Get(profile())->IsExtensionDisabled(extension->id()));
  EXPECT_TRUE(service()->IsExtensionEnabled(theme_crx));
}

#if defined(OS_CHROMEOS)
// Always fails on ChromeOS: http://crbug.com/79737
#define MAYBE_UpdatePendingExternalCrx DISABLED_UpdatePendingExternalCrx
#else
#define MAYBE_UpdatePendingExternalCrx UpdatePendingExternalCrx
#endif
// Test updating a pending CRX as if the source is an external extension
// with an update URL.  In this case we don't know if the CRX is a theme
// or not.
TEST_F(ExtensionServiceTest, MAYBE_UpdatePendingExternalCrx) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      theme_crx,
      std::string(),
      GURL(),
      Manifest::EXTERNAL_PREF_DOWNLOAD,
      Extension::NO_FLAGS,
      false));

  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension = service()->GetExtensionById(theme_crx, true);
  ASSERT_TRUE(extension);

  EXPECT_FALSE(
      ExtensionPrefs::Get(profile())->IsExtensionDisabled(extension->id()));
  EXPECT_TRUE(service()->IsExtensionEnabled(extension->id()));
  EXPECT_FALSE(
      extensions::util::IsIncognitoEnabled(extension->id(), profile()));
}

// Test updating a pending CRX as if the source is an external extension
// with an update URL.  The external update should overwrite a sync update,
// but a sync update should not overwrite a non-sync update.
TEST_F(ExtensionServiceTest, UpdatePendingExternalCrxWinsOverSync) {
  InitializeEmptyExtensionService();

  // Add a crx to be installed from the update mechanism.
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsExtension,
          kGoodRemoteInstall,
          kGoodInstalledByCustodian));

  // Check that there is a pending crx, with is_from_sync set to true.
  const extensions::PendingExtensionInfo* pending_extension_info;
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_TRUE(pending_extension_info->is_from_sync());

  // Add a crx to be updated, with the same ID, from a non-sync source.
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      kGoodId,
      std::string(),
      GURL(kGoodUpdateURL),
      Manifest::EXTERNAL_PREF_DOWNLOAD,
      Extension::NO_FLAGS,
      false));

  // Check that there is a pending crx, with is_from_sync set to false.
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_FALSE(pending_extension_info->is_from_sync());
  EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD,
            pending_extension_info->install_source());

  // Add a crx to be installed from the update mechanism.
  EXPECT_FALSE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsExtension,
          kGoodRemoteInstall,
          kGoodInstalledByCustodian));

  // Check that the external, non-sync update was not overridden.
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_FALSE(pending_extension_info->is_from_sync());
  EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD,
            pending_extension_info->install_source());
}

// Updating a theme should fail if the updater is explicitly told that
// the CRX is not a theme.
TEST_F(ExtensionServiceTest, UpdatePendingCrxThemeMismatch) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromSync(
      theme_crx, GURL(), base::Version(), &IsExtension, false, false));

  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, FAILED_SILENTLY);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension = service()->GetExtensionById(theme_crx, true);
  ASSERT_FALSE(extension);
}

// TODO(akalin): Test updating a pending extension non-silently once
// we can mock out ExtensionInstallUI and inject our version into
// UpdateExtension().

// Test updating a pending extension which fails the should-install test.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionFailedShouldInstallTest) {
  InitializeEmptyExtensionService();
  // Add pending extension with a flipped is_theme.
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsTheme,
          kGoodRemoteInstall,
          kGoodInstalledByCustodian));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  // TODO(akalin): Figure out how to check that the extensions
  // directory is cleaned up properly in OnExtensionInstalled().

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

// TODO(akalin): Figure out how to test that installs of pending
// unsyncable extensions are blocked.

// Test updating a pending extension for one that is not pending.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionNotPending) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

// Test updating a pending extension for one that is already
// installed.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionAlreadyInstalled) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());

  EXPECT_FALSE(good->is_theme());

  // Use AddExtensionImpl() as AddFrom*() would balk.
  service()->pending_extension_manager()->AddExtensionImpl(
      good->id(),
      std::string(),
      extensions::ManifestURL::GetUpdateURL(good),
      Version(),
      &IsExtension,
      kGoodIsFromSync,
      Manifest::INTERNAL,
      Extension::NO_FLAGS,
      false,
      kGoodRemoteInstall);
  UpdateExtension(good->id(), path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

#if defined(ENABLE_BLACKLIST_TESTS)
// Tests blacklisting then unblacklisting extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, SetUnsetBlacklistInPrefs) {
  extensions::TestBlacklist test_blacklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);
  service()->Init();

  const extensions::ExtensionSet& enabled_extensions =
      registry()->enabled_extensions();
  const extensions::ExtensionSet& blacklisted_extensions =
      registry()->blacklisted_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(good0) &&
              !blacklisted_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1) &&
              !blacklisted_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2) &&
              !blacklisted_extensions.Contains(good2));

  EXPECT_FALSE(IsPrefExist(good0, "blacklist"));
  EXPECT_FALSE(IsPrefExist(good1, "blacklist"));
  EXPECT_FALSE(IsPrefExist(good2, "blacklist"));
  EXPECT_FALSE(IsPrefExist("invalid_id", "blacklist"));

  // Blacklist good0 and good1 (and an invalid extension ID).
  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_MALWARE, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_MALWARE, true);
  test_blacklist.SetBlacklistState(
      "invalid_id", extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!enabled_extensions.Contains(good0) &&
              blacklisted_extensions.Contains(good0));
  EXPECT_TRUE(!enabled_extensions.Contains(good1) &&
              blacklisted_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2) &&
              !blacklisted_extensions.Contains(good2));

  EXPECT_TRUE(ValidateBooleanPref(good0, "blacklist", true));
  EXPECT_TRUE(ValidateBooleanPref(good1, "blacklist", true));
  EXPECT_FALSE(IsPrefExist(good2, "blacklist"));
  EXPECT_FALSE(IsPrefExist("invalid_id", "blacklist"));

  // Un-blacklist good1 and blacklist good2.
  test_blacklist.Clear(false);
  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_MALWARE, true);
  test_blacklist.SetBlacklistState(
      good2, extensions::BLACKLISTED_MALWARE, true);
  test_blacklist.SetBlacklistState(
      "invalid_id", extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!enabled_extensions.Contains(good0) &&
              blacklisted_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1) &&
              !blacklisted_extensions.Contains(good1));
  EXPECT_TRUE(!enabled_extensions.Contains(good2) &&
              blacklisted_extensions.Contains(good2));

  EXPECT_TRUE(ValidateBooleanPref(good0, "blacklist", true));
  EXPECT_FALSE(IsPrefExist(good1, "blacklist"));
  EXPECT_TRUE(ValidateBooleanPref(good2, "blacklist", true));
  EXPECT_FALSE(IsPrefExist("invalid_id", "blacklist"));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Tests trying to install a blacklisted extension.
TEST_F(ExtensionServiceTest, BlacklistedExtensionWillNotInstall) {
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blacklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_db(blacklist_db);

  InitializeEmptyExtensionService();
  service()->Init();

  // After blacklisting good_crx, we cannot install it.
  blacklist_db->SetUnsafe(good_crx).NotifyUpdate();
  base::RunLoop().RunUntilIdle();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // HACK: specify WAS_INSTALLED_BY_DEFAULT so that test machinery doesn't
  // decide to install this silently. Somebody should fix these tests, all
  // 6,000 lines of them. Hah!
  InstallCRX(path, INSTALL_FAILED, Extension::WAS_INSTALLED_BY_DEFAULT);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Unload blacklisted extension on policy change.
TEST_F(ExtensionServiceTest, UnloadBlacklistedExtensionPolicy) {
  extensions::TestBlacklist test_blacklist;

  // A profile with no extensions installed.
  InitializeEmptyExtensionServiceWithTestingPrefs();
  test_blacklist.Attach(service()->blacklist_);

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good_crx, true);
  }

  test_blacklist.SetBlacklistState(
      good_crx, extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  // The good_crx is blacklisted and the whitelist doesn't negate it.
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "blacklist", true));
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Tests that a blacklisted extension is eventually unloaded on startup, if it
// wasn't already.
TEST_F(ExtensionServiceTest, WillNotLoadBlacklistedExtensionsFromDirectory) {
  extensions::TestBlacklist test_blacklist;

  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);

  // Blacklist good1 before the service initializes.
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_MALWARE, false);

  // Load extensions.
  service()->Init();
  ASSERT_EQ(3u, loaded_.size());  // hasn't had time to blacklist yet

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, registry()->blacklisted_extensions().size());
  ASSERT_EQ(2u, registry()->enabled_extensions().size());

  ASSERT_TRUE(registry()->enabled_extensions().Contains(good0));
  ASSERT_TRUE(registry()->blacklisted_extensions().Contains(good1));
  ASSERT_TRUE(registry()->enabled_extensions().Contains(good2));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Tests extensions blacklisted in prefs on startup; one still blacklisted by
// safe browsing, the other not. The not-blacklisted one should recover.
TEST_F(ExtensionServiceTest, BlacklistedInPrefsFromStartup) {
  extensions::TestBlacklist test_blacklist;

  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);
  ExtensionPrefs::Get(profile())->SetExtensionBlacklisted(good0, true);
  ExtensionPrefs::Get(profile())->SetExtensionBlacklisted(good1, true);

  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_MALWARE, false);

  // Extension service hasn't loaded yet, but IsExtensionEnabled reads out of
  // prefs. Ensure it takes into account the blacklist state (crbug.com/373842).
  EXPECT_FALSE(service()->IsExtensionEnabled(good0));
  EXPECT_FALSE(service()->IsExtensionEnabled(good1));
  EXPECT_TRUE(service()->IsExtensionEnabled(good2));

  service()->Init();

  EXPECT_EQ(2u, registry()->blacklisted_extensions().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  EXPECT_TRUE(registry()->blacklisted_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blacklisted_extensions().Contains(good1));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));

  // Give time for the blacklist to update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, registry()->blacklisted_extensions().size());
  EXPECT_EQ(2u, registry()->enabled_extensions().size());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blacklisted_extensions().Contains(good1));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Extension is added to blacklist with BLACKLISTED_POTENTIALLY_UNWANTED state
// after it is installed. It is then successfully re-enabled by the user.
TEST_F(ExtensionServiceTest, GreylistedExtensionDisabled) {
  extensions::TestBlacklist test_blacklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);
  service()->Init();

  const extensions::ExtensionSet& enabled_extensions =
      registry()->enabled_extensions();
  const extensions::ExtensionSet& disabled_extensions =
      registry()->disabled_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));

  // Blacklist good0 and good1 (and an invalid extension ID).
  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_CWS_POLICY_VIOLATION, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_POTENTIALLY_UNWANTED, true);
  test_blacklist.SetBlacklistState(
      "invalid_id", extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(enabled_extensions.Contains(good0));
  EXPECT_TRUE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));
  EXPECT_FALSE(disabled_extensions.Contains(good2));

  ValidateIntegerPref(
      good0, "blacklist_state", extensions::BLACKLISTED_CWS_POLICY_VIOLATION);
  ValidateIntegerPref(
      good1, "blacklist_state", extensions::BLACKLISTED_POTENTIALLY_UNWANTED);

  // Now user enables good0.
  service()->EnableExtension(good0);

  EXPECT_TRUE(enabled_extensions.Contains(good0));
  EXPECT_FALSE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));

  // Remove extensions from blacklist.
  test_blacklist.SetBlacklistState(
      good0, extensions::NOT_BLACKLISTED, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::NOT_BLACKLISTED, true);
  base::RunLoop().RunUntilIdle();

  // All extensions are enabled.
  EXPECT_TRUE(enabled_extensions.Contains(good0));
  EXPECT_FALSE(disabled_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1));
  EXPECT_FALSE(disabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));
  EXPECT_FALSE(disabled_extensions.Contains(good2));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// When extension is removed from greylist, do not re-enable it if it is
// disabled by user.
TEST_F(ExtensionServiceTest, GreylistDontEnableManuallyDisabled) {
  extensions::TestBlacklist test_blacklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);
  service()->Init();

  const extensions::ExtensionSet& enabled_extensions =
      registry()->enabled_extensions();
  const extensions::ExtensionSet& disabled_extensions =
      registry()->disabled_extensions();

  // Manually disable.
  service()->DisableExtension(good0,
                              extensions::Extension::DISABLE_USER_ACTION);

  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_CWS_POLICY_VIOLATION, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_POTENTIALLY_UNWANTED, true);
  test_blacklist.SetBlacklistState(
      good2, extensions::BLACKLISTED_SECURITY_VULNERABILITY, true);
  base::RunLoop().RunUntilIdle();

  // All extensions disabled.
  EXPECT_FALSE(enabled_extensions.Contains(good0));
  EXPECT_TRUE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));
  EXPECT_FALSE(enabled_extensions.Contains(good2));
  EXPECT_TRUE(disabled_extensions.Contains(good2));

  // Greylisted extension can be enabled.
  service()->EnableExtension(good1);
  EXPECT_TRUE(enabled_extensions.Contains(good1));
  EXPECT_FALSE(disabled_extensions.Contains(good1));

  // good1 is now manually disabled.
  service()->DisableExtension(good1,
                              extensions::Extension::DISABLE_USER_ACTION);
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));

  // Remove extensions from blacklist.
  test_blacklist.SetBlacklistState(
      good0, extensions::NOT_BLACKLISTED, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::NOT_BLACKLISTED, true);
  test_blacklist.SetBlacklistState(
      good2, extensions::NOT_BLACKLISTED, true);
  base::RunLoop().RunUntilIdle();

  // good0 and good1 remain disabled.
  EXPECT_FALSE(enabled_extensions.Contains(good0));
  EXPECT_TRUE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));
  EXPECT_FALSE(disabled_extensions.Contains(good2));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

#if defined(ENABLE_BLACKLIST_TESTS)
// Blacklisted extension with unknown state are not enabled/disabled.
TEST_F(ExtensionServiceTest, GreylistUnknownDontChange) {
  extensions::TestBlacklist test_blacklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);
  service()->Init();

  const extensions::ExtensionSet& enabled_extensions =
      registry()->enabled_extensions();
  const extensions::ExtensionSet& disabled_extensions =
      registry()->disabled_extensions();

  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_CWS_POLICY_VIOLATION, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_POTENTIALLY_UNWANTED, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(enabled_extensions.Contains(good0));
  EXPECT_TRUE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));
  EXPECT_FALSE(disabled_extensions.Contains(good2));

  test_blacklist.SetBlacklistState(
      good0, extensions::NOT_BLACKLISTED, true);
  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_UNKNOWN, true);
  test_blacklist.SetBlacklistState(
      good2, extensions::BLACKLISTED_UNKNOWN, true);
  base::RunLoop().RunUntilIdle();

  // good0 re-enabled, other remain as they were.
  EXPECT_TRUE(enabled_extensions.Contains(good0));
  EXPECT_FALSE(disabled_extensions.Contains(good0));
  EXPECT_FALSE(enabled_extensions.Contains(good1));
  EXPECT_TRUE(disabled_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2));
  EXPECT_FALSE(disabled_extensions.Contains(good2));
}

// Tests that blacklisted extensions cannot be reloaded, both those loaded
// before and after extension service startup.
TEST_F(ExtensionServiceTest, ReloadBlacklistedExtension) {
  extensions::TestBlacklist test_blacklist;

  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);

  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_MALWARE, false);
  service()->Init();
  test_blacklist.SetBlacklistState(
      good2, extensions::BLACKLISTED_MALWARE, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(StringSet(good0), registry()->enabled_extensions().GetIDs());
  EXPECT_EQ(StringSet(good1, good2),
            registry()->blacklisted_extensions().GetIDs());

  service()->ReloadExtension(good1);
  service()->ReloadExtension(good2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(StringSet(good0), registry()->enabled_extensions().GetIDs());
  EXPECT_EQ(StringSet(good1, good2),
            registry()->blacklisted_extensions().GetIDs());
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

// Tests blocking then unblocking enabled extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockEnabledExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking disabled extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockDisabledExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  service()->DisableExtension(good0, Extension::DISABLE_RELOAD);

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking terminated extensions after the service has
// been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockTerminatedExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  TerminateExtension(good0);

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking policy-forced extensions after the service has
// been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockPolicyExtension) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // // Blacklist everything.
    // pref.SetBlacklistedByDefault(true);
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  // Have policy force-install an extension.
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(
      good_crx, "1.0.0.0", data_dir().AppendASCII("good_crx"));

  // Reloading extensions should find our externally registered extension
  // and install it.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  AssertExtensionBlocksAndUnblocks(false, good_crx);
}


#if defined(ENABLE_BLACKLIST_TESTS)
// Tests blocking then unblocking extensions that are blacklisted both before
// and after Init().
TEST_F(ExtensionServiceTest, BlockAndUnblockBlacklistedExtension) {
  extensions::TestBlacklist test_blacklist;

  InitializeGoodInstalledExtensionService();
  test_blacklist.Attach(service()->blacklist_);

  test_blacklist.SetBlacklistState(
      good0, extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  service()->Init();

  test_blacklist.SetBlacklistState(
      good1, extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  // Blacklisted extensions stay blacklisted.
  AssertExtensionBlocksAndUnblocks(false, good0);
  AssertExtensionBlocksAndUnblocks(false, good1);

  service()->BlockAllExtensions();

  // Remove an extension from the blacklist while the service is blocked.
  test_blacklist.SetBlacklistState(
      good0, extensions::NOT_BLACKLISTED, true);
  // Add an extension to the blacklist while the service is blocked.
  test_blacklist.SetBlacklistState(
      good2, extensions::BLACKLISTED_MALWARE, true);
  base::RunLoop().RunUntilIdle();

  // Go directly to blocked, do not pass go, do not collect $200.
  ASSERT_TRUE(IsBlocked(good0));
  // Get on the blacklist - even if you were blocked!
  ASSERT_FALSE(IsBlocked(good2));
}
#endif  // defined(ENABLE_BLACKLIST_TESTS)

// Tests blocking then unblocking enabled component extensions after the service
// has been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockEnabledComponentExtension) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install a component extension.
  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII(good0)
                            .AppendASCII("1.0.0.0");
  std::string manifest;
  ASSERT_TRUE(base::ReadFileToString(
      path.Append(extensions::kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Component extension should never block.
  AssertExtensionBlocksAndUnblocks(false, good0);
}

// Tests blocking then unblocking a theme after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  InstallCRX(path, INSTALL_NEW);

  AssertExtensionBlocksAndUnblocks(true, theme_crx);
}

// Tests that blocking extensions before Init() results in loading blocked
// extensions.
TEST_F(ExtensionServiceTest, WillNotLoadExtensionsWhenBlocked) {
  InitializeGoodInstalledExtensionService();

  service()->BlockAllExtensions();

  service()->Init();

  ASSERT_TRUE(IsBlocked(good0));
  ASSERT_TRUE(IsBlocked(good0));
  ASSERT_TRUE(IsBlocked(good0));
}

// Tests that IsEnabledExtension won't crash on an uninstalled extension.
TEST_F(ExtensionServiceTest, IsEnabledExtensionBlockedAndNotInstalled) {
  InitializeEmptyExtensionService();

  service()->BlockAllExtensions();

  service()->IsExtensionEnabled(theme_crx);
}

// Will not install extension blacklisted by policy.
TEST_F(ExtensionServiceTest, BlacklistedByPolicyWillNotInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Blacklist everything.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetBlacklistedByDefault(true);
  }

  // Blacklist prevents us from installing good_crx.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_FAILED);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // Now whitelist this particular extension.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good_crx, true);
  }

  // Ensure we can now install good_crx.
  InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
}

// Extension blacklisted by policy get unloaded after installing.
TEST_F(ExtensionServiceTest, BlacklistedByPolicyRemovedIfRunning) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install good_crx.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Blacklist this extension.
    pref.SetIndividualExtensionInstallationAllowed(good_crx, false);
  }

  // Extension should not be running now.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that component extensions are not blacklisted by policy.
TEST_F(ExtensionServiceTest, ComponentExtensionWhitelisted) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Blacklist everything.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetBlacklistedByDefault(true);
  }

  // Install a component extension.
  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII(good0)
                            .AppendASCII("1.0.0.0");
  std::string manifest;
  ASSERT_TRUE(base::ReadFileToString(
      path.Append(extensions::kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Extension should be installed despite blacklist.
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good0, false));

  // Poke external providers and make sure the extension is still present.
  service()->CheckForExternalUpdates();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good0, false));

  // Extension should not be uninstalled on blacklist changes.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good0, false);
  }
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good0, false));
}

// Tests that policy-installed extensions are not blacklisted by policy.
TEST_F(ExtensionServiceTest, PolicyInstalledExtensionsWhitelisted) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Blacklist everything.
    pref.SetBlacklistedByDefault(true);
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  // Have policy force-install an extension.
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(
      good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));

  // Reloading extensions should find our externally registered extension
  // and install it.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  // Extension should be installed despite blacklist.
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));

  // Blacklist update should not uninstall the extension.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good0, false);
  }
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
}

// Tests that extensions cannot be installed if the policy provider prohibits
// it. This functionality is implemented in CrxInstaller::ConfirmInstall().
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsInstall) {
  InitializeEmptyExtensionService();

  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider_(
      extensions::TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider_);

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_FAILED);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that extensions cannot be loaded from prefs if the policy provider
// prohibits it. This functionality is implemented in InstalledLoader::Load().
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsLoadFromPrefs) {
  InitializeEmptyExtensionService();

  // Create a fake extension to be loaded as though it were read from prefs.
  base::FilePath path =
      data_dir().AppendASCII("management").AppendASCII("simple_extension");
  base::DictionaryValue manifest;
  manifest.SetString(keys::kName, "simple_extension");
  manifest.SetString(keys::kVersion, "1");
  // UNPACKED is for extensions loaded from a directory. We use it here, even
  // though we're testing loading from prefs, so that we don't need to provide
  // an extension key.
  extensions::ExtensionInfo extension_info(
      &manifest, std::string(), path, Manifest::UNPACKED);

  // Ensure we can load it with no management policy in place.
  GetManagementPolicy()->UnregisterAllProviders();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  extensions::InstalledLoader(service()).Load(extension_info, false);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  const Extension* extension =
      (registry()->enabled_extensions().begin())->get();
  EXPECT_TRUE(
      service()->UninstallExtension(extension->id(),
                                    extensions::UNINSTALL_REASON_FOR_TESTING,
                                    base::Bind(&base::DoNothing),
                                    NULL));
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // Ensure we cannot load it if management policy prohibits installation.
  extensions::TestManagementPolicyProvider provider_(
      extensions::TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider_);

  extensions::InstalledLoader(service()).Load(extension_info, false);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests disabling an extension when prohibited by the ManagementPolicy.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsDisable) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempt to disable it.
  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests uninstalling an extension when prohibited by the ManagementPolicy.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsUninstall) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempt to uninstall it.
  EXPECT_FALSE(
      service()->UninstallExtension(good_crx,
                                    extensions::UNINSTALL_REASON_FOR_TESTING,
                                    base::Bind(&base::DoNothing),
                                    NULL));

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
}

// Tests that previously installed extensions that are now prohibited from
// being installed are removed.
TEST_F(ExtensionServiceTest, ManagementPolicyUnloadsAllProhibited) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("page_action.crx"), INSTALL_NEW);
  EXPECT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Run the policy check.
  service()->CheckManagementPolicy();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests that previously disabled extensions that are now required to be
// enabled are re-enabled on reinstall.
TEST_F(ExtensionServiceTest, ManagementPolicyRequiresEnable) {
  InitializeEmptyExtensionService();

  // Install, then disable, an extension.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  // Register an ExtensionManagementPolicy that requires the extension to remain
  // enabled.
  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Reinstall the extension.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_UPDATED);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests that extensions disabled by management policy can be installed but
// will get disabled after installing.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsEnableOnInstalled) {
  InitializeEmptyExtensionService();

  // Register an ExtensionManagementPolicy that disables all extensions, with
  // a specified Extension::DisableReason.
  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::MUST_REMAIN_DISABLED);
  provider.SetDisableReason(Extension::DISABLE_NOT_VERIFIED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempts to install an extensions, it should be installed but disabled.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_WITHOUT_LOAD);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  // Verifies that the disable reason is set properly.
  EXPECT_EQ(Extension::DISABLE_NOT_VERIFIED,
            service()->extension_prefs_->GetDisableReasons(kGoodId));
}

// Tests that extensions with conflicting required permissions by enterprise
// policy cannot be installed.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionNewExtensionInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");

  {
    // Update policy to block one of the required permissions of target.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "tabs");
  }

  // The extension should be failed to install.
  PackAndInstallCRX(path, INSTALL_FAILED);

  {
    // Update policy to block one of the optional permissions instead.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*", "history");
  }

  // The extension should succeed to install this time.
  std::string id = PackAndInstallCRX(path, INSTALL_NEW)->id();

  // Uninstall the extension and update policy to block some arbitrary
  // unknown permission.
  UninstallExtension(id, false);
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*", "unknown.permission.for.testing");
  }

  // The extension should succeed to install as well.
  PackAndInstallCRX(path, INSTALL_NEW);
}

// Tests that extension supposed to be force installed but with conflicting
// required permissions cannot be installed.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionConflictsWithForceInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Pack the crx file.
  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir.path().AppendASCII("temp.crx");

  PackCRX(path, pem_path, crx_path);

  {
    // Block one of the required permissions.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "tabs");
  }

  // Use MockExtensionProvider to simulate force installing extension.
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(permissions_blocklist, "1.0", crx_path);

  {
    // Attempts to force install this extension.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    service()->CheckForExternalUpdates();
    observer.Wait();
  }

  // The extension should not be installed.
  ASSERT_FALSE(service()->GetInstalledExtension(permissions_blocklist));

  // Remove this extension from pending extension manager as we would like to
  // give another attempt later.
  service()->pending_extension_manager()->Remove(permissions_blocklist);

  {
    // Clears the permission block list.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
  }

  {
    // Attempts to force install this extension again.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    service()->CheckForExternalUpdates();
    observer.Wait();
  }

  const Extension* installed =
      service()->GetInstalledExtension(permissions_blocklist);
  ASSERT_TRUE(installed);
  EXPECT_EQ(installed->location(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
}

// Tests that newer versions of an extension with conflicting required
// permissions by enterprise policy cannot be updated to.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionExtensionUpdate) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath path2 = data_dir().AppendASCII("permissions_blocklist2");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");

  // Install 'permissions_blocklist'.
  const Extension* installed = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  EXPECT_EQ(installed->id(), permissions_blocklist);

  {
    // Block one of the required permissions of 'permissions_blocklist2'.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "downloads");
  }

  // Install 'permissions_blocklist' again, should be updated.
  const Extension* updated = PackAndInstallCRX(path, pem_path, INSTALL_UPDATED);
  EXPECT_EQ(updated->id(), permissions_blocklist);

  std::string old_version = updated->VersionString();

  // Attempts to update to 'permissions_blocklist2' should fail.
  PackAndInstallCRX(path2, pem_path, INSTALL_FAILED);

  // Verify that the old version is still enabled.
  updated = service()->GetExtensionById(permissions_blocklist, false);
  ASSERT_TRUE(updated);
  EXPECT_EQ(old_version, updated->VersionString());
}

// Tests that policy update with additional permissions blocked revoke
// conflicting granted optional permissions and unload extensions with
// conflicting required permissions, including the force installed ones.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionPolicyUpdate) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath path2 = data_dir().AppendASCII("permissions_blocklist2");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");

  // Pack the crx file.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir.path().AppendASCII("temp.crx");

  PackCRX(path2, pem_path, crx_path);

  // Install two arbitary extensions with specified manifest.
  std::string ext1 = PackAndInstallCRX(path, INSTALL_NEW)->id();
  std::string ext2 = PackAndInstallCRX(path2, INSTALL_NEW)->id();
  ASSERT_NE(ext1, permissions_blocklist);
  ASSERT_NE(ext2, permissions_blocklist);
  ASSERT_NE(ext1, ext2);

  // Force install another extension with known id and same manifest as 'ext2'.
  std::string ext2_forced = permissions_blocklist;
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(ext2_forced, "2.0", crx_path);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());

  // Verify all three extensions are installed and enabled.
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext1));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext2));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext2_forced));

  // Grant all optional permissions to each extension.
  GrantAllOptionalPermissions(ext1);
  GrantAllOptionalPermissions(ext2);
  GrantAllOptionalPermissions(ext2_forced);

  scoped_ptr<const PermissionSet> active_permissions =
      ExtensionPrefs::Get(profile())->GetActivePermissions(ext1);
  EXPECT_TRUE(active_permissions->HasAPIPermission(
      extensions::APIPermission::kDownloads));

  // Set policy to block 'downloads' permission.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "downloads");
  }

  base::RunLoop().RunUntilIdle();

  // 'ext1' should still be enabled, but with 'downloads' permission revoked.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(ext1));
  active_permissions =
      ExtensionPrefs::Get(profile())->GetActivePermissions(ext1);
  EXPECT_FALSE(active_permissions->HasAPIPermission(
      extensions::APIPermission::kDownloads));

  // 'ext2' should be disabled because one of its required permissions is
  // blocked.
  EXPECT_FALSE(registry->enabled_extensions().GetByID(ext2));

  // 'ext2_forced' should be handled the same as 'ext2'
  EXPECT_FALSE(registry->enabled_extensions().GetByID(ext2_forced));
}

// Flaky on windows; http://crbug.com/309833
#if defined(OS_WIN)
#define MAYBE_ExternalExtensionAutoAcknowledgement DISABLED_ExternalExtensionAutoAcknowledgement
#else
#define MAYBE_ExternalExtensionAutoAcknowledgement ExternalExtensionAutoAcknowledgement
#endif
TEST_F(ExtensionServiceTest, MAYBE_ExternalExtensionAutoAcknowledgement) {
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(true);

  {
    // Register and install an external extension.
    MockExtensionProvider* provider =
        new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
    AddMockExternalProvider(provider);
    provider->UpdateOrAddExtension(
        good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));
  }
  {
    // Have policy force-install an extension.
    MockExtensionProvider* provider = new MockExtensionProvider(
        service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
    AddMockExternalProvider(provider);
    provider->UpdateOrAddExtension(
        page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));
  }

  // Providers are set up. Let them run.
  int count = 2;
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      base::Bind(&WaitForCountNotificationsCallback, &count));
  service()->CheckForExternalUpdates();

  observer.Wait();

  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
  EXPECT_TRUE(service()->GetExtensionById(page_action, false));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(!prefs->IsExternalExtensionAcknowledged(good_crx));
  ASSERT_TRUE(prefs->IsExternalExtensionAcknowledged(page_action));
}

#if !defined(OS_CHROMEOS)
// This tests if default apps are installed correctly.
TEST_F(ExtensionServiceTest, DefaultAppsInstall) {
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(true);

  {
    std::string json_data =
        "{"
        "  \"ldnnhddmnhbkjipkidpdiheffobcpfmf\" : {"
        "    \"external_crx\": \"good.crx\","
        "    \"external_version\": \"1.0.0.0\","
        "    \"is_bookmark_app\": false"
        "  }"
        "}";
    default_apps::Provider* provider = new default_apps::Provider(
        profile(),
        service(),
        new extensions::ExternalTestingLoader(json_data, data_dir()),
        Manifest::INTERNAL,
        Manifest::INVALID_LOCATION,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT);

    AddMockExternalProvider(provider);
  }

  ASSERT_EQ(0u, registry()->enabled_extensions().size());
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
  const Extension* extension = service()->GetExtensionById(good_crx, false);
  EXPECT_TRUE(extension->from_webstore());
  EXPECT_TRUE(extension->was_installed_by_default());
}
#endif

// Tests disabling extensions
TEST_F(ExtensionServiceTest, DisableExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(service()->GetExtensionById(good_crx, true));
  EXPECT_TRUE(service()->GetExtensionById(good_crx, false));

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());

  // Disable it.
  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);

  EXPECT_TRUE(service()->GetExtensionById(good_crx, true));
  EXPECT_FALSE(service()->GetExtensionById(good_crx, false));
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());
}

TEST_F(ExtensionServiceTest, TerminateExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());

  TerminateExtension(good_crx);

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(1u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());
}

TEST_F(ExtensionServiceTest, DisableTerminatedExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  EXPECT_TRUE(registry()->GetExtensionById(
      good_crx, extensions::ExtensionRegistry::TERMINATED));

  // Disable it.
  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);

  EXPECT_FALSE(registry()->GetExtensionById(
      good_crx, extensions::ExtensionRegistry::TERMINATED));
  EXPECT_TRUE(service()->GetExtensionById(good_crx, true));

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());
}

// Tests disabling all extensions (simulating --disable-extensions flag).
TEST_F(ExtensionServiceTest, DisableAllExtensions) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // Disable extensions.
  service()->set_extensions_enabled(false);
  service()->ReloadExtensionsForTest();

  // There shouldn't be extensions in either list.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // This shouldn't do anything when all extensions are disabled.
  service()->EnableExtension(good_crx);
  service()->ReloadExtensionsForTest();

  // There still shouldn't be extensions in either list.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // And then re-enable the extensions.
  service()->set_extensions_enabled(true);
  service()->ReloadExtensionsForTest();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests reloading extensions.
TEST_F(ExtensionServiceTest, ReloadExtensions) {
  InitializeEmptyExtensionService();

  // Simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW,
             Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT);
  const char* const extension_id = good_crx;
  service()->DisableExtension(extension_id, Extension::DISABLE_USER_ACTION);

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  service()->ReloadExtensionsForTest();

  // The creation flags should not change when reloading the extension.
  const Extension* extension = service()->GetExtensionById(good_crx, true);
  EXPECT_TRUE(extension->from_webstore());
  EXPECT_TRUE(extension->was_installed_by_default());
  EXPECT_FALSE(extension->from_bookmark());

  // Extension counts shouldn't change.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  service()->EnableExtension(extension_id);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // Need to clear |loaded_| manually before reloading as the
  // EnableExtension() call above inserted into it and
  // UnloadAllExtensions() doesn't send out notifications.
  loaded_.clear();
  service()->ReloadExtensionsForTest();

  // Extension counts shouldn't change.
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests reloading an extension.
TEST_F(ExtensionServiceTest, ReloadExtension) {
  InitializeEmptyExtensionService();

  // Simple extension that should install without error.
  const char extension_id[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  base::FilePath ext = data_dir()
                           .AppendASCII("good")
                           .AppendASCII("Extensions")
                           .AppendASCII(extension_id)
                           .AppendASCII("1.0.0.0");
  extensions::UnpackedInstaller::Create(service())->Load(ext);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  service()->ReloadExtension(extension_id);

  // Extension should be disabled now, waiting to be reloaded.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(Extension::DISABLE_RELOAD,
            ExtensionPrefs::Get(profile())->GetDisableReasons(extension_id));

  // Reloading again should not crash.
  service()->ReloadExtension(extension_id);

  // Finish reloading
  base::RunLoop().RunUntilIdle();

  // Extension should be enabled again.
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

TEST_F(ExtensionServiceTest, UninstallExtension) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  UninstallExtension(good_crx, false);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(UnloadedExtensionInfo::REASON_UNINSTALL, unloaded_reason_);
}

TEST_F(ExtensionServiceTest, UninstallTerminatedExtension) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  UninstallExtension(good_crx, false);
  EXPECT_EQ(UnloadedExtensionInfo::REASON_TERMINATE, unloaded_reason_);
}

// Tests the uninstaller helper.
TEST_F(ExtensionServiceTest, UninstallExtensionHelper) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  UninstallExtension(good_crx, true);
  EXPECT_EQ(UnloadedExtensionInfo::REASON_UNINSTALL, unloaded_reason_);
}

TEST_F(ExtensionServiceTest, UninstallExtensionHelperTerminated) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  UninstallExtension(good_crx, true);
  EXPECT_EQ(UnloadedExtensionInfo::REASON_TERMINATE, unloaded_reason_);
}

// An extension disabled because of unsupported requirements should re-enabled
// if updated to a version with supported requirements as long as there are no
// other disable reasons.
TEST_F(ExtensionServiceTest, UpgradingRequirementsEnabled) {
  InitializeEmptyExtensionService();
  BlackListWebGL();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  EXPECT_TRUE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements"),
          pem_path,
          v2_bad_requirements_crx);
  UpdateExtension(id, v2_bad_requirements_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_good_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_good"), pem_path, v3_good_crx);
  UpdateExtension(id, v3_good_crx, ENABLED);
  EXPECT_TRUE(service()->IsExtensionEnabled(id));
}

// Extensions disabled through user action should stay disabled.
TEST_F(ExtensionServiceTest, UpgradingRequirementsDisabled) {
  InitializeEmptyExtensionService();
  BlackListWebGL();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  service()->DisableExtension(id, Extension::DISABLE_USER_ACTION);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements"),
          pem_path,
          v2_bad_requirements_crx);
  UpdateExtension(id, v2_bad_requirements_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_good_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_good"), pem_path, v3_good_crx);
  UpdateExtension(id, v3_good_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));
}

// The extension should not re-enabled because it was disabled from a
// permission increase.
TEST_F(ExtensionServiceTest, UpgradingRequirementsPermissions) {
  InitializeEmptyExtensionService();
  BlackListWebGL();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  EXPECT_TRUE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_and_permissions_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements_and_permissions"),
          pem_path,
          v2_bad_requirements_and_permissions_crx);
  UpdateExtension(id, v2_bad_requirements_and_permissions_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_bad_permissions_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_bad_permissions"),
          pem_path,
          v3_bad_permissions_crx);
  UpdateExtension(id, v3_bad_permissions_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));
}

// Unpacked extensions are not allowed to be installed if they have unsupported
// requirements.
TEST_F(ExtensionServiceTest, UnpackedRequirements) {
  InitializeEmptyExtensionService();
  BlackListWebGL();

  base::FilePath path =
      data_dir().AppendASCII("requirements").AppendASCII("v2_bad_requirements");
  extensions::UnpackedInstaller::Create(service())->Load(path);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

class ExtensionCookieCallback {
 public:
  ExtensionCookieCallback()
    : result_(false),
      weak_factory_(base::MessageLoop::current()) {}

  void SetCookieCallback(bool result) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&base::MessageLoop::QuitWhenIdle,
                              weak_factory_.GetWeakPtr()));
    result_ = result;
  }

  void GetAllCookiesCallback(const net::CookieList& list) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&base::MessageLoop::QuitWhenIdle,
                              weak_factory_.GetWeakPtr()));
    list_ = list;
  }
  net::CookieList list_;
  bool result_;
  base::WeakPtrFactory<base::MessageLoop> weak_factory_;
};

// Verifies extension state is removed upon uninstall.
TEST_F(ExtensionServiceTest, ClearExtensionData) {
  InitializeEmptyExtensionService();
  ExtensionCookieCallback callback;

  // Load a test extension.
  base::FilePath path = data_dir();
  path = path.AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  GURL ext_url(extension->url());
  std::string origin_id = storage::GetIdentifierFromOrigin(ext_url);

  // Set a cookie for the extension.
  net::CookieStore* cookie_store = profile()->GetRequestContextForExtensions()
                                            ->GetURLRequestContext()
                                            ->cookie_store();
  ASSERT_TRUE(cookie_store);
  net::CookieOptions options;
  cookie_store->SetCookieWithOptionsAsync(
       ext_url, "dummy=value", options,
       base::Bind(&ExtensionCookieCallback::SetCookieCallback,
                  base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback.result_);

  cookie_store->GetAllCookiesForURLAsync(
      ext_url,
      base::Bind(&ExtensionCookieCallback::GetAllCookiesCallback,
                 base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, callback.list_.size());

  // Open a database.
  storage::DatabaseTracker* db_tracker =
      BrowserContext::GetDefaultStoragePartition(profile())
          ->GetDatabaseTracker();
  base::string16 db_name = base::UTF8ToUTF16("db");
  base::string16 description = base::UTF8ToUTF16("db_description");
  int64_t size;
  db_tracker->DatabaseOpened(origin_id, db_name, description, 1, &size);
  db_tracker->DatabaseClosed(origin_id, db_name);
  std::vector<storage::OriginInfo> origins;
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(origin_id, origins[0].GetOriginIdentifier());

  // Create local storage. We only simulate this by creating the backing files.
  // Note: This test depends on details of how the dom_storage library
  // stores data in the host file system.
  base::FilePath lso_dir_path =
      profile()->GetPath().AppendASCII("Local Storage");
  base::FilePath lso_file_path = lso_dir_path.AppendASCII(origin_id)
      .AddExtension(FILE_PATH_LITERAL(".localstorage"));
  EXPECT_TRUE(base::CreateDirectory(lso_dir_path));
  EXPECT_EQ(0, base::WriteFile(lso_file_path, NULL, 0));
  EXPECT_TRUE(base::PathExists(lso_file_path));

  // Create indexed db. Similarly, it is enough to only simulate this by
  // creating the directory on the disk.
  IndexedDBContext* idb_context = BrowserContext::GetDefaultStoragePartition(
                                      profile())->GetIndexedDBContext();
  idb_context->SetTaskRunnerForTesting(
      base::MessageLoop::current()->task_runner().get());
  base::FilePath idb_path = idb_context->GetFilePathForTesting(origin_id);
  EXPECT_TRUE(base::CreateDirectory(idb_path));
  EXPECT_TRUE(base::DirectoryExists(idb_path));

  // Uninstall the extension.
  base::RunLoop run_loop;
  ASSERT_TRUE(
      service()->UninstallExtension(good_crx,
                                    extensions::UNINSTALL_REASON_FOR_TESTING,
                                    run_loop.QuitClosure(),
                                    NULL));
  // The data deletion happens on the IO thread.
  run_loop.Run();

  // Check that the cookie is gone.
  cookie_store->GetAllCookiesForURLAsync(
       ext_url,
       base::Bind(&ExtensionCookieCallback::GetAllCookiesCallback,
                  base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, callback.list_.size());

  // The database should have vanished as well.
  origins.clear();
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(0U, origins.size());

  // Check that the LSO file has been removed.
  EXPECT_FALSE(base::PathExists(lso_file_path));

  // Check if the indexed db has disappeared too.
  EXPECT_FALSE(base::DirectoryExists(idb_path));
}

// Verifies app state is removed upon uninstall.
TEST_F(ExtensionServiceTest, ClearAppData) {
  InitializeEmptyExtensionService();
  ExtensionCookieCallback callback;

  int pref_count = 0;

  // Install app1 with unlimited storage.
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  const std::string id1 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kUnlimitedStorage));
  const GURL origin1(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));
  std::string origin_id = storage::GetIdentifierFromOrigin(origin1);

  // Install app2 from the same origin with unlimited storage.
  extension = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin2(
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin());
  EXPECT_EQ(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin2));

  // Set a cookie for the extension.
  net::CookieStore* cookie_store = profile()->GetRequestContext()
                                            ->GetURLRequestContext()
                                            ->cookie_store();
  ASSERT_TRUE(cookie_store);
  net::CookieOptions options;
  cookie_store->SetCookieWithOptionsAsync(
       origin1, "dummy=value", options,
       base::Bind(&ExtensionCookieCallback::SetCookieCallback,
                  base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback.result_);

  cookie_store->GetAllCookiesForURLAsync(
      origin1,
      base::Bind(&ExtensionCookieCallback::GetAllCookiesCallback,
                 base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, callback.list_.size());

  // Open a database.
  storage::DatabaseTracker* db_tracker =
      BrowserContext::GetDefaultStoragePartition(profile())
          ->GetDatabaseTracker();
  base::string16 db_name = base::UTF8ToUTF16("db");
  base::string16 description = base::UTF8ToUTF16("db_description");
  int64_t size;
  db_tracker->DatabaseOpened(origin_id, db_name, description, 1, &size);
  db_tracker->DatabaseClosed(origin_id, db_name);
  std::vector<storage::OriginInfo> origins;
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(origin_id, origins[0].GetOriginIdentifier());

  // Create local storage. We only simulate this by creating the backing files.
  // Note: This test depends on details of how the dom_storage library
  // stores data in the host file system.
  base::FilePath lso_dir_path =
      profile()->GetPath().AppendASCII("Local Storage");
  base::FilePath lso_file_path = lso_dir_path.AppendASCII(origin_id)
      .AddExtension(FILE_PATH_LITERAL(".localstorage"));
  EXPECT_TRUE(base::CreateDirectory(lso_dir_path));
  EXPECT_EQ(0, base::WriteFile(lso_file_path, NULL, 0));
  EXPECT_TRUE(base::PathExists(lso_file_path));

  // Create indexed db. Similarly, it is enough to only simulate this by
  // creating the directory on the disk.
  IndexedDBContext* idb_context = BrowserContext::GetDefaultStoragePartition(
                                      profile())->GetIndexedDBContext();
  idb_context->SetTaskRunnerForTesting(
      base::MessageLoop::current()->task_runner().get());
  base::FilePath idb_path = idb_context->GetFilePathForTesting(origin_id);
  EXPECT_TRUE(base::CreateDirectory(idb_path));
  EXPECT_TRUE(base::DirectoryExists(idb_path));

  // Uninstall one of them, unlimited storage should still be granted
  // to the origin.
  UninstallExtension(id1, false);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  // Check that the cookie is still there.
  cookie_store->GetAllCookiesForURLAsync(
       origin1,
       base::Bind(&ExtensionCookieCallback::GetAllCookiesCallback,
                  base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, callback.list_.size());

  // Now uninstall the other. Storage should be cleared for the apps.
  UninstallExtension(id2, false);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          origin1));

  // Check that the cookie is gone.
  cookie_store->GetAllCookiesForURLAsync(
       origin1,
       base::Bind(&ExtensionCookieCallback::GetAllCookiesCallback,
                  base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, callback.list_.size());

  // The database should have vanished as well.
  origins.clear();
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(0U, origins.size());

  // Check that the LSO file has been removed.
  EXPECT_FALSE(base::PathExists(lso_file_path));

  // Check if the indexed db has disappeared too.
  EXPECT_FALSE(base::DirectoryExists(idb_path));
}

// Tests loading single extensions (like --load-extension)
// Flaky crashes. http://crbug.com/231806
TEST_F(ExtensionServiceTest, DISABLED_LoadExtension) {
  InitializeEmptyExtensionService();

  base::FilePath ext1 = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                            .AppendASCII("1.0.0.0");
  extensions::UnpackedInstaller::Create(service())->Load(ext1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Manifest::UNPACKED, loaded_[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  ValidatePrefKeyCount(1);

  base::FilePath no_manifest =
      data_dir()
          .AppendASCII("bad")
          // .AppendASCII("Extensions")
          .AppendASCII("cccccccccccccccccccccccccccccccc")
          .AppendASCII("1");
  extensions::UnpackedInstaller::Create(service())->Load(no_manifest);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  // Test uninstall.
  std::string id = loaded_[0]->id();
  EXPECT_FALSE(unloaded_id_.length());
  service()->UninstallExtension(id,
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing),
                                NULL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(id, unloaded_id_);
  ASSERT_EQ(0u, loaded_.size());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that we generate IDs when they are not specified in the manifest for
// --load-extension.
TEST_F(ExtensionServiceTest, GenerateID) {
  InitializeEmptyExtensionService();

  base::FilePath no_id_ext = data_dir().AppendASCII("no_id");
  extensions::UnpackedInstaller::Create(service())->Load(no_id_ext);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_TRUE(crx_file::id_util::IdIsValid(loaded_[0]->id()));
  EXPECT_EQ(loaded_[0]->location(), Manifest::UNPACKED);

  ValidatePrefKeyCount(1);

  std::string previous_id = loaded_[0]->id();

  // If we reload the same path, we should get the same extension ID.
  extensions::UnpackedInstaller::Create(service())->Load(no_id_ext);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(previous_id, loaded_[0]->id());
}

TEST_F(ExtensionServiceTest, UnpackedValidatesLocales) {
  InitializeEmptyExtensionService();

  base::FilePath bad_locale =
      data_dir().AppendASCII("unpacked").AppendASCII("bad_messages_file");
  extensions::UnpackedInstaller::Create(service())->Load(bad_locale);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  base::FilePath ms_messages_file = bad_locale.AppendASCII("_locales")
                                              .AppendASCII("ms")
                                              .AppendASCII("messages.json");
  EXPECT_THAT(base::UTF16ToUTF8(GetErrors()[0]), testing::AllOf(
       testing::HasSubstr(
           base::UTF16ToUTF8(ms_messages_file.LossyDisplayName())),
       testing::HasSubstr("Dictionary keys must be quoted.")));
  ASSERT_EQ(0u, loaded_.size());
}

void ExtensionServiceTest::TestExternalProvider(
    MockExtensionProvider* provider, Manifest::Location location) {
  // Verify that starting with no providers loads no extensions.
  service()->Init();
  ASSERT_EQ(0u, loaded_.size());

  provider->set_visit_count(0);

  // Register a test extension externally using the mock registry provider.
  base::FilePath source_path = data_dir().AppendASCII("good.crx");

  // Add the extension.
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Reloading extensions should find our externally registered extension
  // and install it.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(location, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  // Reload extensions without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service()->ReloadExtensionsForTest();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  content::WindowedNotificationObserver observer_2(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer_2.Wait();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  // Uninstall the extension and reload. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_[0]->id();
  bool no_uninstall =
      GetManagementPolicy()->MustRemainEnabled(loaded_[0].get(), NULL);
  service()->UninstallExtension(id,
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing),
                                NULL);
  base::RunLoop().RunUntilIdle();

  base::FilePath install_path = extensions_install_dir().AppendASCII(id);
  if (no_uninstall) {
    // Policy controlled extensions should not have been touched by uninstall.
    ASSERT_TRUE(base::PathExists(install_path));
  } else {
    // The extension should also be gone from the install directory.
    ASSERT_FALSE(base::PathExists(install_path));
    loaded_.clear();
    service()->CheckForExternalUpdates();
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(0u, loaded_.size());
    ValidatePrefKeyCount(1);
    ValidateIntegerPref(good_crx, "state",
                        Extension::EXTERNAL_EXTENSION_UNINSTALLED);
    ValidateIntegerPref(good_crx, "location", location);

    // Now clear the preference and reinstall.
    SetPrefInteg(good_crx, "state", Extension::ENABLED);

    loaded_.clear();
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    service()->CheckForExternalUpdates();
    observer.Wait();
    ASSERT_EQ(1u, loaded_.size());
  }
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  if (GetManagementPolicy()->MustRemainEnabled(loaded_[0].get(), NULL)) {
    EXPECT_EQ(2, provider->visit_count());
  } else {
    // Now test an externally triggered uninstall (deleting the registry key or
    // the pref entry).
    provider->RemoveExtension(good_crx);

    loaded_.clear();
    service()->OnExternalProviderReady(provider);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(0u, loaded_.size());
    ValidatePrefKeyCount(0);

    // The extension should also be gone from the install directory.
    ASSERT_FALSE(base::PathExists(install_path));

    // Now test the case where user uninstalls and then the extension is removed
    // from the external provider.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);
    service()->CheckForExternalUpdates();
    observer.Wait();

    ASSERT_EQ(1u, loaded_.size());
    ASSERT_EQ(0u, GetErrors().size());

    // User uninstalls.
    loaded_.clear();
    service()->UninstallExtension(id,
                                  extensions::UNINSTALL_REASON_FOR_TESTING,
                                  base::Bind(&base::DoNothing),
                                  NULL);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(0u, loaded_.size());

    // Then remove the extension from the extension provider.
    provider->RemoveExtension(good_crx);

    // Should still be at 0.
    loaded_.clear();
    extensions::InstalledLoader(service()).LoadAllExtensions();
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(0u, loaded_.size());
    ValidatePrefKeyCount(1);

    EXPECT_EQ(5, provider->visit_count());
  }
}

// Tests the external installation feature
#if defined(OS_WIN)
TEST_F(ExtensionServiceTest, ExternalInstallRegistry) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(false);

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* reg_provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_REGISTRY);
  AddMockExternalProvider(reg_provider);
  TestExternalProvider(reg_provider, Manifest::EXTERNAL_REGISTRY);
}
#endif

TEST_F(ExtensionServiceTest, ExternalInstallPref) {
  InitializeEmptyExtensionService();

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);

  AddMockExternalProvider(pref_provider);
  TestExternalProvider(pref_provider, Manifest::EXTERNAL_PREF);
}

TEST_F(ExtensionServiceTest, ExternalInstallPrefUpdateUrl) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(false);

  // TODO(skerner): The mock provider is not a good model of a provider
  // that works with update URLs, because it adds file and version info.
  // Extend the mock to work with update URLs.  This test checks the
  // behavior that is common to all external extension visitors.  The
  // browser test ExtensionManagementTest.ExternalUrlUpdate tests that
  // what the visitor does results in an extension being downloaded and
  // installed.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF_DOWNLOAD);
  AddMockExternalProvider(pref_provider);
  TestExternalProvider(pref_provider, Manifest::EXTERNAL_PREF_DOWNLOAD);
}

TEST_F(ExtensionServiceTest, ExternalInstallPolicyUpdateUrl) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(false);

  // TODO(skerner): The mock provider is not a good model of a provider
  // that works with update URLs, because it adds file and version info.
  // Extend the mock to work with update URLs. This test checks the
  // behavior that is common to all external extension visitors. The
  // browser test ExtensionManagementTest.ExternalUrlUpdate tests that
  // what the visitor does results in an extension being downloaded and
  // installed.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  AddMockExternalProvider(pref_provider);
  TestExternalProvider(pref_provider, Manifest::EXTERNAL_POLICY_DOWNLOAD);
}

// Tests that external extensions get uninstalled when the external extension
// providers can't account for them.
TEST_F(ExtensionServiceTest, ExternalUninstall) {
  // Start the extensions service with one external extension already installed.
  base::FilePath source_install_dir =
      data_dir().AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("PreferencesExternal");

  // This initializes the extensions service with no ExternalProviders.
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  service()->set_extensions_enabled(false);

  service()->Init();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());

  // Verify that it's not the disabled extensions flag causing it not to load.
  service()->set_extensions_enabled(true);
  service()->ReloadExtensionsForTest();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());
}

// Test that running multiple update checks simultaneously does not
// keep the update from succeeding.
TEST_F(ExtensionServiceTest, MultipleExternalUpdateCheck) {
  InitializeEmptyExtensionService();

  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);

  // Verify that starting with no providers loads no extensions.
  service()->Init();
  ASSERT_EQ(0u, loaded_.size());

  // Start two checks for updates.
  provider->set_visit_count(0);
  service()->CheckForExternalUpdates();
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();

  // Two calls should cause two checks for external extensions.
  EXPECT_EQ(2, provider->visit_count());
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());

  // Register a test extension externally using the mock registry provider.
  base::FilePath source_path = data_dir().AppendASCII("good.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Two checks for external updates should find the extension, and install it
  // once.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  provider->set_visit_count(0);
  service()->CheckForExternalUpdates();
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_EQ(2, provider->visit_count());
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Manifest::EXTERNAL_PREF, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::EXTERNAL_PREF);

  provider->RemoveExtension(good_crx);
  provider->set_visit_count(0);
  service()->CheckForExternalUpdates();
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();

  // Two calls should cause two checks for external extensions.
  // Because the external source no longer includes good_crx,
  // good_crx will be uninstalled.  So, expect that no extensions
  // are loaded.
  EXPECT_EQ(2, provider->visit_count());
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());
}

TEST_F(ExtensionServiceTest, ExternalPrefProvider) {
  InitializeEmptyExtensionService();

  // Test some valid extension records.
  // Set a base path to avoid erroring out on relative paths.
  // Paths starting with // are absolute on every platform we support.
  base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockProviderVisitor visitor(base_path);
  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  }"
      "}";
  EXPECT_EQ(3, visitor.Visit(json_data));

  // Simulate an external_extensions.json file that contains seven invalid
  // records:
  // - One that is missing the 'external_crx' key.
  // - One that is missing the 'external_version' key.
  // - One that is specifying .. in the path.
  // - One that specifies both a file and update URL.
  // - One that specifies no file or update URL.
  // - One that has an update URL that is not well formed.
  // - One that contains a malformed version.
  // - One that has an invalid id.
  // - One that has a non-dictionary value.
  // - One that has an integer 'external_version' instead of a string.
  // The final extension is valid, and we check that it is read to make sure
  // failures don't stop valid records from being read.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension.crx\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"..\\\\foo\\\\RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"dddddddddddddddddddddddddddddddd\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\","
      "    \"external_update_url\": \"http:\\\\foo.com/update\""
      "  },"
      "  \"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\": {"
      "  },"
      "  \"ffffffffffffffffffffffffffffffff\": {"
      "    \"external_update_url\": \"This string is not a valid URL\""
      "  },"
      "  \"gggggggggggggggggggggggggggggggg\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"This is not a valid version!\""
      "  },"
      "  \"This is not a valid id!\": {},"
      "  \"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh\": true,"
      "  \"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii\": {"
      "    \"external_crx\": \"RandomExtension4.crx\","
      "    \"external_version\": 1.0"
      "  },"
      "  \"pppppppppppppppppppppppppppppppp\": {"
      "    \"external_crx\": \"RandomValidExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  }"
      "}";
  EXPECT_EQ(1, visitor.Visit(json_data));

  // Check that if a base path is not provided, use of a relative
  // path fails.
  base::FilePath empty;
  MockProviderVisitor visitor_no_relative_paths(empty);

  // Use absolute paths.  Expect success.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"//RandomExtension1.crx\","
      "    \"external_version\": \"3.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"//path/to/RandomExtension2.crx\","
      "    \"external_version\": \"3.0\""
      "  }"
      "}";
  EXPECT_EQ(2, visitor_no_relative_paths.Visit(json_data));

  // Use a relative path.  Expect that it will error out.
  json_data =
      "{"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"3.0\""
      "  }"
      "}";
  EXPECT_EQ(0, visitor_no_relative_paths.Visit(json_data));

  // Test supported_locales.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"supported_locales\": [ \"en\" ]"
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\","
      "    \"supported_locales\": [ \"en-GB\" ]"
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"3.0\","
      "    \"supported_locales\": [ \"en_US\", \"fr\" ]"
      "  }"
      "}";
  {
    ScopedBrowserLocale guard("en-US");
    EXPECT_EQ(2, visitor.Visit(json_data));
  }

  // Test keep_if_present.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"keep_if_present\": true"
      "  }"
      "}";
  {
    EXPECT_EQ(0, visitor.Visit(json_data));
  }

  // Test is_bookmark_app.
  MockProviderVisitor from_bookmark_visitor(
      base_path, Extension::FROM_BOOKMARK);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"is_bookmark_app\": true"
      "  }"
      "}";
  EXPECT_EQ(1, from_bookmark_visitor.Visit(json_data));

  // Test is_from_webstore.
  MockProviderVisitor from_webstore_visitor(
      base_path, Extension::FROM_WEBSTORE);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"is_from_webstore\": true"
      "  }"
      "}";
  EXPECT_EQ(1, from_webstore_visitor.Visit(json_data));

  // Test was_installed_by_eom.
  MockProviderVisitor was_installed_by_eom_visitor(
      base_path, Extension::WAS_INSTALLED_BY_OEM);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"was_installed_by_oem\": true"
      "  }"
      "}";
  EXPECT_EQ(1, was_installed_by_eom_visitor.Visit(json_data));

  // Test min_profile_created_by_version.
  MockProviderVisitor min_profile_created_by_version_visitor(base_path);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"min_profile_created_by_version\": \"42.0.0.1\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"1.0\","
      "    \"min_profile_created_by_version\": \"43.0.0.1\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"3.0\","
      "    \"min_profile_created_by_version\": \"44.0.0.1\""
      "  }"
      "}";
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "40.0.0.1");
  EXPECT_EQ(0, min_profile_created_by_version_visitor.Visit(json_data));
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "43.0.0.1");
  EXPECT_EQ(2, min_profile_created_by_version_visitor.Visit(json_data));
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "45.0.0.1");
  EXPECT_EQ(3, min_profile_created_by_version_visitor.Visit(json_data));
}

TEST_F(ExtensionServiceTest, DoNotInstallForEnterprise) {
  InitializeEmptyExtensionService();

  const base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockProviderVisitor visitor(base_path);
  policy::ProfilePolicyConnector* const connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
          visitor.profile());
  connector->OverrideIsManagedForTesting(true);
  EXPECT_TRUE(connector->IsManaged());

  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"do_not_install_for_enterprise\": true"
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"1.0\""
      "  }"
      "}";
  EXPECT_EQ(1, visitor.Visit(json_data));
}

TEST_F(ExtensionServiceTest, IncrementalUpdateThroughRegistry) {
  InitializeEmptyExtensionService();

  // Test some valid extension records.
  // Set a base path to avoid erroring out on relative paths.
  // Paths starting with // are absolute on every platform we support.
  base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockUpdateProviderVisitor visitor(base_path);
  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  }"
      "}";
  EXPECT_EQ(3, visitor.Visit(json_data, Manifest::EXTERNAL_REGISTRY,
                             Manifest::EXTERNAL_PREF_DOWNLOAD));

  // c* removed and d*, e*, f* added, a*, b* existing.
  json_data =
      "{"
      "  \"dddddddddddddddddddddddddddddddd\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  },"
      "  \"ffffffffffffffffffffffffffffffff\": {"
      "    \"external_update_url\": \"http:\\\\bar.com/update\","
      "    \"install_parameter\": \"id\""
      "  },"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  }"
      "}";

  // This will simulate registry loader observing new changes in registry and
  // hence will discover new extensions.
  visitor.VisitDueToUpdate(json_data);

  // UpdateUrl.
  EXPECT_EQ(2u, visitor.GetUpdateURLExtensionCount());
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithUpdateUrl("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithUpdateUrl("ffffffffffffffffffffffffffffffff"));

  // File.
  EXPECT_EQ(3u, visitor.GetFileExtensionCount());
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("dddddddddddddddddddddddddddddddd"));

  // Removed extensions.
  EXPECT_EQ(1u, visitor.GetRemovedExtensionCount());
  EXPECT_TRUE(visitor.HasSeenRemoval("cccccccccccccccccccccccccccccccc"));

  // Simulate all 5 extensions being removed.
  json_data = "{}";
  visitor.VisitDueToUpdate(json_data);
  EXPECT_EQ(0u, visitor.GetUpdateURLExtensionCount());
  EXPECT_EQ(0u, visitor.GetFileExtensionCount());
  EXPECT_EQ(5u, visitor.GetRemovedExtensionCount());
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAndRelocalizeExtensions) {
  // Ensure we're testing in "en" and leave global state untouched.
  extension_l10n_util::ScopedLocaleForTest testLocale("en");

  // Initialize the test dir with a good Preferences/extensions.
  base::FilePath source_install_dir = data_dir().AppendASCII("l10n");
  base::FilePath pref_path =
      source_install_dir.Append(chrome::kPreferencesFilename);
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service()->Init();

  ASSERT_EQ(3u, loaded_.size());

  // This was equal to "sr" on load.
  ValidateStringPref(loaded_[0]->id(), keys::kCurrentLocale, "en");

  // These are untouched by re-localization.
  ValidateStringPref(loaded_[1]->id(), keys::kCurrentLocale, "en");
  EXPECT_FALSE(IsPrefExist(loaded_[1]->id(), keys::kCurrentLocale));

  // This one starts with Serbian name, and gets re-localized into English.
  EXPECT_EQ("My name is simple.", loaded_[0]->name());

  // These are untouched by re-localization.
  EXPECT_EQ("My name is simple.", loaded_[1]->name());
  EXPECT_EQ("no l10n", loaded_[2]->name());
}

class ExtensionsReadyRecorder : public content::NotificationObserver {
 public:
  ExtensionsReadyRecorder() : ready_(false) {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                   content::NotificationService::AllSources());
  }

  void set_ready(bool value) { ready_ = value; }
  bool ready() { return ready_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED:
        ready_ = true;
        break;
      default:
        NOTREACHED();
    }
  }

  content::NotificationRegistrar registrar_;
  bool ready_;
};

// Test that we get enabled/disabled correctly for all the pref/command-line
// combinations. We don't want to derive from the ExtensionServiceTest class
// for this test, so we use ExtensionServiceTestSimple.
//
// Also tests that we always fire EXTENSIONS_READY, no matter whether we are
// enabled or not.
class ExtensionServiceTestSimple : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ExtensionServiceTestSimple, Enabledness) {
  // Make sure the PluginService singleton is destroyed at the end of the test.
  base::ShadowingAtExitManager at_exit_manager;
#if defined(ENABLE_PLUGINS)
  content::PluginService::GetInstance()->Init();
  content::PluginService::GetInstance()->DisablePluginsDiscoveryForTesting();
#endif

  ExtensionErrorReporter::Init(false);  // no noisy errors
  ExtensionsReadyRecorder recorder;
  scoped_ptr<TestingProfile> profile(new TestingProfile());
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService device_settings_service;
  chromeos::ScopedTestCrosSettings cros_settings;
  scoped_ptr<chromeos::ScopedTestUserManager> user_manager(
      new chromeos::ScopedTestUserManager);
#endif
  scoped_ptr<base::CommandLine> command_line;
  base::FilePath install_dir = profile->GetPath()
      .AppendASCII(extensions::kInstallDirectoryName);

  // By default, we are enabled.
  command_line.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
  ExtensionService* service = static_cast<extensions::TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->
      CreateExtensionService(
          command_line.get(),
          install_dir,
          false);
  EXPECT_TRUE(service->extensions_enabled());
  service->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recorder.ready());
#if defined OS_CHROMEOS
  user_manager.reset();
#endif

  // If either the command line or pref is set, we are disabled.
  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  command_line->AppendSwitch(switches::kDisableExtensions);
  service = static_cast<extensions::TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->
      CreateExtensionService(
          command_line.get(),
          install_dir,
          false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
  service = static_cast<extensions::TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->
      CreateExtensionService(
          command_line.get(),
          install_dir,
          false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
  command_line.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
  service = static_cast<extensions::TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->
      CreateExtensionService(
          command_line.get(),
          install_dir,
          false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recorder.ready());

  // Explicitly delete all the resources used in this test.
  profile.reset();
  service = NULL;
  // Execute any pending deletion tasks.
  base::RunLoop().RunUntilIdle();
}

// Test loading extensions that require limited and unlimited storage quotas.
TEST_F(ExtensionServiceTest, StorageQuota) {
  InitializeEmptyExtensionService();

  base::FilePath extensions_path = data_dir().AppendASCII("storage_quota");

  base::FilePath limited_quota_ext =
      extensions_path.AppendASCII("limited_quota")
      .AppendASCII("1.0");

  // The old permission name for unlimited quota was "unlimited_storage", but
  // we changed it to "unlimitedStorage". This tests both versions.
  base::FilePath unlimited_quota_ext =
      extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("1.0");
  base::FilePath unlimited_quota_ext2 =
      extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("2.0");
  extensions::UnpackedInstaller::Create(service())->Load(limited_quota_ext);
  extensions::UnpackedInstaller::Create(service())->Load(unlimited_quota_ext);
  extensions::UnpackedInstaller::Create(service())->Load(unlimited_quota_ext2);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3u, loaded_.size());
  EXPECT_TRUE(profile());
  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          loaded_[0]->url()));
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      loaded_[1]->url()));
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      loaded_[2]->url()));
}

// Tests ComponentLoader::Add().
TEST_F(ExtensionServiceTest, ComponentExtensions) {
  InitializeEmptyExtensionService();

  // Component extensions should work even when extensions are disabled.
  service()->set_extensions_enabled(false);

  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                            .AppendASCII("1.0.0.0");

  std::string manifest;
  ASSERT_TRUE(base::ReadFileToString(
      path.Append(extensions::kManifestFilename), &manifest));

  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Note that we do not pump messages -- the extension should be loaded
  // immediately.

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Manifest::COMPONENT, loaded_[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  // Component extensions get a prefs entry on first install.
  ValidatePrefKeyCount(1);

  // Reload all extensions, and make sure it comes back.
  std::string extension_id = (*registry()->enabled_extensions().begin())->id();
  loaded_.clear();
  service()->ReloadExtensionsForTest();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(extension_id, (*registry()->enabled_extensions().begin())->id());
}

TEST_F(ExtensionServiceTest, InstallPriorityExternalUpdateUrl) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(1u);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::INTERNAL);

  extensions::PendingExtensionManager* pending =
      service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Skip install when the location is the same.
  scoped_ptr<GURL> good_update_url(new GURL(kGoodUpdateURL));
  scoped_ptr<ExternalInstallInfoUpdateUrl> info(
      new ExternalInstallInfoUpdateUrl(
          kGoodId, std::string(), std::move(good_update_url),
          Manifest::INTERNAL, Extension::NO_FLAGS, false));
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(*info, true));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Install when the location has higher priority.
  info->download_location = Manifest::EXTERNAL_POLICY_DOWNLOAD;
  EXPECT_TRUE(service()->OnExternalExtensionUpdateUrlFound(*info, true));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // Try the low priority again.  Should be rejected.
  info->download_location = Manifest::EXTERNAL_PREF_DOWNLOAD;
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(*info, true));
  // The existing record should still be present in the pending extension
  // manager.
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  pending->Remove(kGoodId);

  // Skip install when the location has the same priority as the installed
  // location.
  info->download_location = Manifest::INTERNAL;
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(*info, true));

  EXPECT_FALSE(pending->IsIdPending(kGoodId));
}

TEST_F(ExtensionServiceTest, InstallPriorityExternalLocalFile) {
  Version older_version("0.1.0.0");
  Version newer_version("2.0.0.0");

  // We don't want the extension to be installed.  A path that doesn't
  // point to a valid CRX ensures this.
  const base::FilePath kInvalidPathToCrx(FILE_PATH_LITERAL("invalid_path"));

  const int kCreationFlags = 0;
  const bool kDontMarkAcknowledged = false;
  const bool kDontInstallImmediately = false;

  InitializeEmptyExtensionService();

  // The test below uses install source constants to test that
  // priority is enforced.  It assumes a specific ranking of install
  // sources: Registry (EXTERNAL_REGISTRY) overrides external pref
  // (EXTERNAL_PREF), and external pref overrides user install (INTERNAL).
  // The following assertions verify these assumptions:
  ASSERT_EQ(Manifest::EXTERNAL_REGISTRY,
            Manifest::GetHigherPriorityLocation(Manifest::EXTERNAL_REGISTRY,
                                                 Manifest::EXTERNAL_PREF));
  ASSERT_EQ(Manifest::EXTERNAL_REGISTRY,
            Manifest::GetHigherPriorityLocation(Manifest::EXTERNAL_REGISTRY,
                                                 Manifest::INTERNAL));
  ASSERT_EQ(Manifest::EXTERNAL_PREF,
            Manifest::GetHigherPriorityLocation(Manifest::EXTERNAL_PREF,
                                                 Manifest::INTERNAL));

  extensions::PendingExtensionManager* pending =
      service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  scoped_ptr<Version> older_version_ptr(new Version(older_version));
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      kGoodId, std::move(older_version_ptr), kInvalidPathToCrx,
      Manifest::INTERNAL, kCreationFlags, kDontMarkAcknowledged,
      kDontInstallImmediately));
  {
    // Simulate an external source adding the extension as INTERNAL.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));
    observer.Wait();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  {
    // Simulate an external source adding the extension as EXTERNAL_PREF.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    info->crx_location = Manifest::EXTERNAL_PREF;
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));
    observer.Wait();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  // Simulate an external source adding as EXTERNAL_PREF again.
  // This is rejected because the version and the location are the same as
  // the previous installation, which is still pending.
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // Try INTERNAL again.  Should fail.
  info->crx_location = Manifest::INTERNAL;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  {
    // Now the registry adds the extension.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    info->crx_location = Manifest::EXTERNAL_REGISTRY;
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));
    observer.Wait();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  // Registry outranks both external pref and internal, so both fail.
  info->crx_location = Manifest::EXTERNAL_PREF;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  info->crx_location = Manifest::INTERNAL;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  pending->Remove(kGoodId);

  // Install the extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* ext = InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(1u);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Manifest::INTERNAL);

  // Now test the logic of OnExternalExtensionFileFound() when the extension
  // being added is already installed.

  // Tests assume |older_version| is less than the installed version, and
  // |newer_version| is greater.  Verify this:
  ASSERT_LT(older_version, *ext->version());
  ASSERT_GT(newer_version, *ext->version());

  // An external install for the same location should fail if the version is
  // older, or the same, and succeed if the version is newer.

  // Older than the installed version...
  info->version.reset(new Version(older_version));
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Same version as the installed version...
  info->version.reset(new Version(ext->VersionString()));
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Newer than the installed version...
  info->version.reset(new Version(newer_version));
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // An external install for a higher priority install source should succeed
  // if the version is greater.  |older_version| is not...
  info->version.reset(new Version(older_version));
  info->crx_location = Manifest::EXTERNAL_PREF;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // |newer_version| is newer.
  info->version.reset(new Version(newer_version));
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // An external install for an even higher priority install source should
  // succeed if the version is greater.
  info->crx_location = Manifest::EXTERNAL_REGISTRY;
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // Because EXTERNAL_PREF is a lower priority source than EXTERNAL_REGISTRY,
  // adding from external pref will now fail.
  info->crx_location = Manifest::EXTERNAL_PREF;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));
}

TEST_F(ExtensionServiceTest, ConcurrentExternalLocalFile) {
  Version kVersion123("1.2.3");
  Version kVersion124("1.2.4");
  Version kVersion125("1.2.5");
  const base::FilePath kInvalidPathToCrx(FILE_PATH_LITERAL("invalid_path"));
  const int kCreationFlags = 0;
  const bool kDontMarkAcknowledged = false;
  const bool kDontInstallImmediately = false;

  InitializeEmptyExtensionService();

  extensions::PendingExtensionManager* pending =
      service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // An external provider starts installing from a local crx.
  scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
      kGoodId, make_scoped_ptr(new Version(kVersion123)), kInvalidPathToCrx,
      Manifest::EXTERNAL_PREF, kCreationFlags, kDontMarkAcknowledged,
      kDontInstallImmediately));
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));

  const extensions::PendingExtensionInfo* pending_info;
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion123);

  // Adding a newer version overrides the currently pending version.
  info->version.reset(new Version(kVersion124));
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding an older version fails.
  info->version.reset(new Version(kVersion123));
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding an older version fails even when coming from a higher-priority
  // location.
  info->crx_location = Manifest::EXTERNAL_REGISTRY;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(*info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding the latest version from the webstore overrides a specific version.
  GURL kUpdateUrl("http://example.com/update");
  scoped_ptr<ExternalInstallInfoUpdateUrl> update_info(
      new ExternalInstallInfoUpdateUrl(
          kGoodId, std::string(), make_scoped_ptr(new GURL(kUpdateUrl)),
          Manifest::EXTERNAL_POLICY_DOWNLOAD, Extension::NO_FLAGS, false));
  EXPECT_TRUE(service()->OnExternalExtensionUpdateUrlFound(*update_info, true));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_FALSE(pending_info->version().IsValid());
}

// This makes sure we can package and install CRX files that use whitelisted
// permissions.
TEST_F(ExtensionServiceTest, InstallWhitelistedExtension) {
  std::string test_id = "hdkklepkcpckhnpgjnmbdfhehckloojk";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      extensions::switches::kWhitelistedExtensionID, test_id);

  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");
  base::FilePath pem_path = path
      .AppendASCII("whitelist.pem");
  path = path
      .AppendASCII("whitelist");

  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(test_id, extension->id());
}

// Test that when multiple sources try to install an extension,
// we consistently choose the right one. To make tests easy to read,
// methods that fake requests to install crx files in several ways
// are provided.
class ExtensionSourcePriorityTest : public ExtensionServiceTest {
 public:
  void SetUp() override {
    ExtensionServiceTest::SetUp();

    // All tests use a single extension.  Put the id and path in member vars
    // that all methods can read.
    crx_id_ = kGoodId;
    crx_path_ = data_dir().AppendASCII("good.crx");
  }

  // Fake an external source adding a URL to fetch an extension from.
  bool AddPendingExternalPrefUrl() {
    return service()->pending_extension_manager()->AddFromExternalUpdateUrl(
        crx_id_,
        std::string(),
        GURL(),
        Manifest::EXTERNAL_PREF_DOWNLOAD,
        Extension::NO_FLAGS,
        false);
  }

  // Fake an external file from external_extensions.json.
  bool AddPendingExternalPrefFileInstall() {
    scoped_ptr<Version> version(new Version("1.0.0.0"));
    scoped_ptr<ExternalInstallInfoFile> info(new ExternalInstallInfoFile(
        crx_id_, std::move(version), crx_path_, Manifest::EXTERNAL_PREF,
        Extension::NO_FLAGS, false, false));
    return service()->OnExternalExtensionFileFound(*info);
  }

  // Fake a request from sync to install an extension.
  bool AddPendingSyncInstall() {
    return service()->pending_extension_manager()->AddFromSync(
        crx_id_,
        GURL(kGoodUpdateURL),
        base::Version(),
        &IsExtension,
        kGoodRemoteInstall,
        kGoodInstalledByCustodian);
  }

  // Fake a policy install.
  bool AddPendingPolicyInstall() {
    // Get path to the CRX with id |kGoodId|.
    scoped_ptr<GURL> empty_url(new GURL());
    scoped_ptr<ExternalInstallInfoUpdateUrl> info(
        new ExternalInstallInfoUpdateUrl(
            crx_id_, std::string(), std::move(empty_url),
            Manifest::EXTERNAL_POLICY_DOWNLOAD, Extension::NO_FLAGS, false));
    return service()->OnExternalExtensionUpdateUrlFound(*info, true);
  }

  // Get the install source of a pending extension.
  Manifest::Location GetPendingLocation() {
    const extensions::PendingExtensionInfo* info;
    EXPECT_TRUE(
        (info = service()->pending_extension_manager()->GetById(crx_id_)));
    return info->install_source();
  }

  // Is an extension pending from a sync request?
  bool GetPendingIsFromSync() {
    const extensions::PendingExtensionInfo* info;
    EXPECT_TRUE(
        (info = service()->pending_extension_manager()->GetById(crx_id_)));
    return info->is_from_sync();
  }

  // Is the CRX id these tests use pending?
  bool IsCrxPending() {
    return service()->pending_extension_manager()->IsIdPending(crx_id_);
  }

  // Is an extension installed?
  bool IsCrxInstalled() {
    return (service()->GetExtensionById(crx_id_, true) != NULL);
  }

 protected:
  // All tests use a single extension.  Making the id and path member
  // vars avoids pasing the same argument to every method.
  std::string crx_id_;
  base::FilePath crx_path_;
};

// Test that a pending request for installation of an external CRX from
// an update URL overrides a pending request to install the same extension
// from sync.
TEST_F(ExtensionSourcePriorityTest, PendingExternalFileOverSync) {
  InitializeEmptyExtensionService();

  ASSERT_FALSE(IsCrxInstalled());

  // Install pending extension from sync.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  EXPECT_TRUE(AddPendingSyncInstall());
  ASSERT_EQ(Manifest::INTERNAL, GetPendingLocation());
  EXPECT_TRUE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  // Install pending as external prefs json would.
  AddPendingExternalPrefFileInstall();
  ASSERT_EQ(Manifest::EXTERNAL_PREF, GetPendingLocation());
  ASSERT_FALSE(IsCrxInstalled());

  // Another request from sync should be ignored.
  EXPECT_FALSE(AddPendingSyncInstall());
  ASSERT_EQ(Manifest::EXTERNAL_PREF, GetPendingLocation());
  ASSERT_FALSE(IsCrxInstalled());

  observer.Wait();
  VerifyCrxInstall(crx_path_, INSTALL_NEW);
  ASSERT_TRUE(IsCrxInstalled());
}

// Test that an install of an external CRX from an update overrides
// an install of the same extension from sync.
TEST_F(ExtensionSourcePriorityTest, PendingExternalUrlOverSync) {
  InitializeEmptyExtensionService();
  ASSERT_FALSE(IsCrxInstalled());

  EXPECT_TRUE(AddPendingSyncInstall());
  ASSERT_EQ(Manifest::INTERNAL, GetPendingLocation());
  EXPECT_TRUE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  ASSERT_TRUE(AddPendingExternalPrefUrl());
  ASSERT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, GetPendingLocation());
  EXPECT_FALSE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  EXPECT_FALSE(AddPendingSyncInstall());
  ASSERT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, GetPendingLocation());
  EXPECT_FALSE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());
}

// Test that an external install request stops sync from installing
// the same extension.
TEST_F(ExtensionSourcePriorityTest, InstallExternalBlocksSyncRequest) {
  InitializeEmptyExtensionService();
  ASSERT_FALSE(IsCrxInstalled());

  // External prefs starts an install.
  AddPendingExternalPrefFileInstall();

  // Crx installer was made, but has not yet run.
  ASSERT_FALSE(IsCrxInstalled());

  // Before the CRX installer runs, Sync requests that the same extension
  // be installed. Should fail, because an external source is pending.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  ASSERT_FALSE(AddPendingSyncInstall());

  // Wait for the external source to install.
  observer.Wait();
  VerifyCrxInstall(crx_path_, INSTALL_NEW);
  ASSERT_TRUE(IsCrxInstalled());

  // Now that the extension is installed, sync request should fail
  // because the extension is already installed.
  ASSERT_FALSE(AddPendingSyncInstall());
}

// Test that installing an external extension displays a GlobalError.
TEST_F(ExtensionServiceTest, ExternalInstallGlobalError) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);

  service()->external_install_manager()->UpdateExternalExtensionAlert();
  // Should return false, meaning there aren't any extensions that the user
  // needs to know about.
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // This is a normal extension, installed normally.
  // This should NOT trigger an alert.
  service()->set_extensions_enabled(true);
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // A hosted app, installed externally.
  // This should NOT trigger an alert.
  provider->UpdateOrAddExtension(
      hosted_app, "1.0.0.0", data_dir().AppendASCII("hosted_app.crx"));

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // Another normal extension, but installed externally.
  // This SHOULD trigger an alert.
  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));

  content::WindowedNotificationObserver observer2(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer2.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
}

// Test that external extensions are initially disabled, and that enabling
// them clears the prompt.
TEST_F(ExtensionServiceTest, ExternalInstallInitiallyDisabled) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);

  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(service()->IsExtensionEnabled(page_action));

  const Extension* extension =
      registry()->disabled_extensions().GetByID(page_action);
  EXPECT_TRUE(extension);
  EXPECT_EQ(page_action, extension->id());

  service()->EnableExtension(page_action);
  EXPECT_FALSE(HasExternalInstallErrors(service()));
  EXPECT_TRUE(service()->IsExtensionEnabled(page_action));
}

// Test that installing multiple external extensions works.
// Flaky on windows; http://crbug.com/295757 .
// Causes race conditions with an in-process utility thread, so disable under
// TSan: https://crbug.com/518957
#if defined(OS_WIN) || defined(THREAD_SANITIZER)
#define MAYBE_ExternalInstallMultiple DISABLED_ExternalInstallMultiple
#else
#define MAYBE_ExternalInstallMultiple ExternalInstallMultiple
#endif
TEST_F(ExtensionServiceTest, MAYBE_ExternalInstallMultiple) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);

  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));
  provider->UpdateOrAddExtension(
      good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));
  provider->UpdateOrAddExtension(
      theme_crx, "2.0", data_dir().AppendASCII("theme.crx"));

  int count = 3;
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      base::Bind(&WaitForCountNotificationsCallback, &count));
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(service()->IsExtensionEnabled(page_action));
  EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
  EXPECT_FALSE(service()->IsExtensionEnabled(theme_crx));

  service()->EnableExtension(page_action);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_TRUE(GetError(good_crx));
  EXPECT_TRUE(GetError(theme_crx));
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));

  service()->EnableExtension(theme_crx);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_FALSE(GetError(theme_crx));
  EXPECT_TRUE(GetError(good_crx));
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));

  service()->EnableExtension(good_crx);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_FALSE(GetError(good_crx));
  EXPECT_FALSE(GetError(theme_crx));
  EXPECT_FALSE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));
}

TEST_F(ExtensionServiceTest, MultipleExternalInstallErrors) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionService();
  service()->set_extensions_enabled(true);

  MockExtensionProvider* reg_provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_REGISTRY);
  AddMockExternalProvider(reg_provider);

  std::string extension_info[][3] = {
      // {id, path, version}
      {good_crx, "1.0.0.0", "good.crx"},
      {page_action, "1.0.0.0", "page_action.crx"},
      {minimal_platform_app_crx, "0.1", "minimal_platform_app.crx"}};

  for (size_t i = 0; i < arraysize(extension_info); ++i) {
    reg_provider->UpdateOrAddExtension(
        extension_info[i][0], extension_info[i][1],
        data_dir().AppendASCII(extension_info[i][2]));
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    service()->CheckForExternalUpdates();
    observer.Wait();
    const size_t expected_error_count = i + 1u;
    EXPECT_EQ(
        expected_error_count,
        service()->external_install_manager()->GetErrorsForTesting().size());
    EXPECT_FALSE(service()->IsExtensionEnabled(extension_info[i][0]));
  }

  std::string extension_ids[] = {
    extension_info[0][0], extension_info[1][0], extension_info[2][0]
  };

  // Each extension should end up in error.
  ASSERT_TRUE(GetError(extension_ids[0]));
  EXPECT_TRUE(GetError(extension_ids[1]));
  EXPECT_TRUE(GetError(extension_ids[2]));

  // Accept the first extension, this will remove the error associated with
  // this extension. Also verify the other errors still exist.
  GetError(extension_ids[0])->OnInstallPromptDone(
      ExtensionInstallPrompt::Result::ACCEPTED);
  EXPECT_FALSE(GetError(extension_ids[0]));
  ASSERT_TRUE(GetError(extension_ids[1]));
  EXPECT_TRUE(GetError(extension_ids[2]));

  // Abort the second extension.
  GetError(extension_ids[1])->OnInstallPromptDone(
      ExtensionInstallPrompt::Result::USER_CANCELED);
  EXPECT_FALSE(GetError(extension_ids[0]));
  EXPECT_FALSE(GetError(extension_ids[1]));
  ASSERT_TRUE(GetError(extension_ids[2]));

  // Finally, re-enable the third extension, all errors should be removed.
  service()->EnableExtension(extension_ids[2]);
  EXPECT_FALSE(GetError(extension_ids[0]));
  EXPECT_FALSE(GetError(extension_ids[1]));
  EXPECT_FALSE(GetError(extension_ids[2]));

  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

// Test that there is a bubble for external extensions that update
// from the webstore if the profile is not new.
TEST_F(ExtensionServiceTest, ExternalInstallUpdatesFromWebstoreOldProfile) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  // This sets up the ExtensionPrefs used by our ExtensionService to be
  // post-first run.
  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.is_first_run = false;
  InitializeExtensionService(params);

  base::FilePath crx_path = temp_dir().path().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  ASSERT_TRUE(GetError(updates_from_webstore));
  EXPECT_EQ(ExternalInstallError::BUBBLE_ALERT,
            GetError(updates_from_webstore)->alert_type());
  EXPECT_FALSE(service()->IsExtensionEnabled(updates_from_webstore));
}

// Test that there is no bubble for external extensions if the profile is new.
TEST_F(ExtensionServiceTest, ExternalInstallUpdatesFromWebstoreNewProfile) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();

  base::FilePath crx_path = temp_dir().path().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExtensionProvider* provider =
      new MockExtensionProvider(service(), Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  ASSERT_TRUE(GetError(updates_from_webstore));
  EXPECT_NE(ExternalInstallError::BUBBLE_ALERT,
            GetError(updates_from_webstore)->alert_type());
  EXPECT_FALSE(service()->IsExtensionEnabled(updates_from_webstore));
}

// Test that clicking to remove the extension on an external install warning
// uninstalls the extension.
TEST_F(ExtensionServiceTest, ExternalInstallClickToRemove) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.is_first_run = false;
  InitializeExtensionService(params);

  base::FilePath crx_path = temp_dir().path().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExtensionProvider* provider =
      new MockExtensionProvider(service_, Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service_->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service_));

  // We check both enabled and disabled, since these are "eventually exclusive"
  // sets.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(updates_from_webstore));

  // Click the negative response.
  service_->external_install_manager()
      ->GetErrorsForTesting()[0]
      ->OnInstallPromptDone(ExtensionInstallPrompt::Result::USER_CANCELED);
  // The Extension should be uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(updates_from_webstore,
                                            ExtensionRegistry::EVERYTHING));
  // The error should be removed.
  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

// Test that clicking to keep the extension on an external install warning
// re-enables the extension.
TEST_F(ExtensionServiceTest, ExternalInstallClickToKeep) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.is_first_run = false;
  InitializeExtensionService(params);

  base::FilePath crx_path = temp_dir().path().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExtensionProvider* provider =
      new MockExtensionProvider(service_, Manifest::EXTERNAL_PREF);
  AddMockExternalProvider(provider);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service_->CheckForExternalUpdates();
  observer.Wait();
  EXPECT_TRUE(HasExternalInstallErrors(service_));

  // We check both enabled and disabled, since these are "eventually exclusive"
  // sets.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(updates_from_webstore));

  // Accept the extension.
  service_->external_install_manager()
      ->GetErrorsForTesting()[0]
      ->OnInstallPromptDone(ExtensionInstallPrompt::Result::ACCEPTED);

  // It should be enabled again.
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(
      registry()->disabled_extensions().GetByID(updates_from_webstore));

  // The error should be removed.
  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

TEST_F(ExtensionServiceTest, InstallBlacklistedExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<Extension> extension = extensions::ExtensionBuilder()
      .SetManifest(extensions::DictionaryBuilder()
          .Set("name", "extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2).Build())
      .Build();
  ASSERT_TRUE(extension.get());
  const std::string& id = extension->id();

  std::set<std::string> id_set;
  id_set.insert(id);

  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile()));
  // Installation should be allowed but the extension should never have been
  // loaded and it should be blacklisted in prefs.
  service()->OnExtensionInstalled(
      extension.get(),
      syncer::StringOrdinal(),
      (extensions::kInstallFlagIsBlacklistedForMalware |
       extensions::kInstallFlagInstallImmediately));
  base::RunLoop().RunUntilIdle();

  // Extension was installed but not loaded.
  observer.WaitForExtensionWillBeInstalled();
  EXPECT_TRUE(service()->GetInstalledExtension(id));

  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(registry()->blacklisted_extensions().Contains(id));

  EXPECT_TRUE(ExtensionPrefs::Get(profile())->IsExtensionBlacklisted(id));
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsBlacklistedExtensionAcknowledged(id));
}

// Tests a profile being destroyed correctly disables extensions.
TEST_F(ExtensionServiceTest, DestroyingProfileClearsExtensions) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_NE(UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN, unloaded_reason_);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());

  service()->Observe(chrome::NOTIFICATION_PROFILE_DESTRUCTION_STARTED,
                     content::Source<Profile>(profile()),
                     content::NotificationService::NoDetails());
  EXPECT_EQ(UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN, unloaded_reason_);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blacklisted_extensions().size());
}
