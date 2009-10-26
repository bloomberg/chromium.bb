// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Extension ids used during testing.
const char* const all_zero = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char* const zero_n_one = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab";
const char* const good0 = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const char* const good1 = "hpiknbiabeeppbpihjehijgoemciehgk";
const char* const good2 = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
const char* const good_crx = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char* const page_action = "obcimlgaoabeegjmmpldobjndiealpln";
const char* const theme_crx = "iamefpfkojoapidjnbafmgkgncegbkad";
const char* const theme2_crx = "pjpgmfcmabopnnfonnhmdjglfpjjfkbf";

struct ExtensionsOrder {
  bool operator()(const Extension* a, const Extension* b) {
    return a->name() < b->name();
  }
};

static std::vector<std::string> GetErrors() {
  const std::vector<std::string>* errors =
      ExtensionErrorReporter::GetInstance()->GetErrors();
  std::vector<std::string> ret_val;

  for (std::vector<std::string>::const_iterator iter = errors->begin();
       iter != errors->end(); ++iter) {
    if (iter->find(".svn") == std::string::npos) {
      ret_val.push_back(*iter);
    }
  }

  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(ret_val.begin(), ret_val.end());

  return ret_val;
}

}  // namespace

class MockExtensionProvider : public ExternalExtensionProvider {
 public:
  explicit MockExtensionProvider(Extension::Location location)
    : location_(location) {}
  virtual ~MockExtensionProvider() {}

  void UpdateOrAddExtension(const std::string& id,
                            const std::string& version,
                            const FilePath& path) {
    extension_map_[id] = std::make_pair(version, path);
  }

  void RemoveExtension(const std::string& id) {
    extension_map_.erase(id);
  }

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const {
    for (DataMap::const_iterator i = extension_map_.begin();
         i != extension_map_.end(); ++i) {
      if (ids_to_ignore.find(i->first) != ids_to_ignore.end())
        continue;
      scoped_ptr<Version> version;
      version.reset(Version::GetVersionFromString(i->second.first));

      visitor->OnExternalExtensionFound(
          i->first, version.get(), i->second.second, location_);
    }
  }

  virtual Version* RegisteredVersion(const std::string& id,
                                     Extension::Location* location) const {
    DataMap::const_iterator it = extension_map_.find(id);
    if (it == extension_map_.end())
      return NULL;

    if (location)
      *location = location_;
    return Version::GetVersionFromString(it->second.first);
  }

 private:
  typedef std::map< std::string, std::pair<std::string, FilePath> > DataMap;
  DataMap extension_map_;
  Extension::Location location_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionProvider);
};

class MockProviderVisitor : public ExternalExtensionProvider::Visitor {
 public:
  MockProviderVisitor() : ids_found_(0) {
  }

  int Visit(const std::string& json_data,
            const std::set<std::string>& ignore_list) {
    // Give the test json file to the provider for parsing.
    provider_.reset(new ExternalPrefExtensionProvider());
    provider_->SetPreferencesForTesting(json_data);

    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    JSONStringValueSerializer serializer(json_data);
    std::string error_msg;
    Value* json_value = serializer.Deserialize(&error_msg);

    if (!error_msg.empty() || !json_value ||
        !json_value->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << L"Unable to deserialize json data";
      return -1;
    } else {
      DictionaryValue* external_extensions =
          static_cast<DictionaryValue*>(json_value);
      prefs_.reset(external_extensions);
    }

    // Reset our counter.
    ids_found_ = 0;
    // Ask the provider to look up all extensions (and return the ones
    // found (that are not on the ignore list).
    provider_->VisitRegisteredExtension(this, ignore_list);

    return ids_found_;
  }

  virtual void OnExternalExtensionFound(const std::string& id,
                                        const Version* version,
                                        const FilePath& path,
                                        Extension::Location unused) {
    ++ids_found_;
    DictionaryValue* pref;
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(prefs_->GetDictionary(ASCIIToWide(id), &pref))
       << L"Got back ID (" << id.c_str() << ") we weren't expecting";

    if (pref) {
      // Ask provider if the extension we got back is registered.
      Extension::Location location = Extension::INVALID;
      scoped_ptr<Version> v1(provider_->RegisteredVersion(id, NULL));
      scoped_ptr<Version> v2(provider_->RegisteredVersion(id, &location));
      EXPECT_STREQ(version->GetString().c_str(), v1->GetString().c_str());
      EXPECT_STREQ(version->GetString().c_str(), v2->GetString().c_str());
      EXPECT_EQ(Extension::EXTERNAL_PREF, location);

      // Remove it so we won't count it ever again.
      prefs_->Remove(ASCIIToWide(id), NULL);
    }
  }

 private:
  int ids_found_;

  scoped_ptr<ExternalPrefExtensionProvider> provider_;
  scoped_ptr<DictionaryValue> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockProviderVisitor);
};

class ExtensionsServiceTest
  : public testing::Test, public NotificationObserver {
 public:
  ExtensionsServiceTest() : installed_(NULL) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::THEME_INSTALLED,
                   NotificationService::AllSources());
  }

  virtual void InitializeExtensionsService(const FilePath& pref_file,
      const FilePath& extensions_install_dir) {
    prefs_.reset(new PrefService(pref_file, NULL));
    profile_.reset(new TestingProfile());
    service_ = new ExtensionsService(profile_.get(),
                                     CommandLine::ForCurrentProcess(),
                                     prefs_.get(),
                                     extensions_install_dir,
                                     &loop_,
                                     &loop_,
                                     false);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_prompts(false);

    // When we start up, we want to make sure there is no external provider,
    // since the ExtensionService on Windows will use the Registry as a default
    // provider and if there is something already registered there then it will
    // interfere with the tests. Those tests that need an external provider
    // will register one specifically.
    service_->ClearProvidersForTesting();

    total_successes_ = 0;
  }

  virtual void InitializeInstalledExtensionsService(const FilePath& prefs_file,
      const FilePath& source_install_dir) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath path_ = temp_dir_.path();
    path_ = path_.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
    file_util::Delete(path_, true);
    file_util::CreateDirectory(path_);
    FilePath temp_prefs = path_.Append(FILE_PATH_LITERAL("Preferences"));
    file_util::CopyFile(prefs_file, temp_prefs);

    extensions_install_dir_ = path_.Append(FILE_PATH_LITERAL("Extensions"));
    file_util::Delete(extensions_install_dir_, true);
    file_util::CopyDirectory(source_install_dir, extensions_install_dir_, true);

    InitializeExtensionsService(temp_prefs, extensions_install_dir_);
  }

  virtual void InitializeEmptyExtensionsService() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath path_ = temp_dir_.path();
    path_ = path_.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
    file_util::Delete(path_, true);
    file_util::CreateDirectory(path_);
    FilePath prefs_filename = path_
        .Append(FILE_PATH_LITERAL("TestPreferences"));
    extensions_install_dir_ = path_.Append(FILE_PATH_LITERAL("Extensions"));
    file_util::Delete(extensions_install_dir_, true);
    file_util::CreateDirectory(extensions_install_dir_);

    InitializeExtensionsService(prefs_filename, extensions_install_dir_);
  }

  static void SetUpTestCase() {
    ExtensionErrorReporter::Init(false);  // no noisy errors
  }

  virtual void SetUp() {
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED: {
        Extension* extension = Details<Extension>(details).ptr();
        loaded_.push_back(extension);
        // The tests rely on the errors being in a certain order, which can vary
        // depending on how filesystem iteration works.
        std::stable_sort(loaded_.begin(), loaded_.end(), ExtensionsOrder());
        break;
      }

      case NotificationType::EXTENSION_UNLOADED: {
        Extension* e = Details<Extension>(details).ptr();
        unloaded_id_ = e->id();
        ExtensionList::iterator i =
            std::find(loaded_.begin(), loaded_.end(), e);
        // TODO(erikkay) fix so this can be an assert.  Right now the tests
        // are manually calling clear() on loaded_, so this isn't doable.
        if (i == loaded_.end())
          return;
        loaded_.erase(i);
        break;
      }
      case NotificationType::EXTENSION_INSTALLED:
      case NotificationType::THEME_INSTALLED:
        installed_ = Details<Extension>(details).ptr();
        break;

      default:
        DCHECK(false);
    }
  }

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

  void SetMockExternalProvider(Extension::Location location,
                               ExternalExtensionProvider* provider) {
    service_->SetProviderForTesting(location, provider);
  }

 protected:
  void InstallExtension(const FilePath& path,
                        bool should_succeed) {
    ASSERT_TRUE(file_util::PathExists(path));
    service_->InstallExtension(path);
    loop_.RunAllPending();
    std::vector<std::string> errors = GetErrors();
    if (should_succeed) {
      ++total_successes_;

      EXPECT_TRUE(installed_) << path.value();

      ASSERT_EQ(1u, loaded_.size()) << path.value();
      EXPECT_EQ(0u, errors.size()) << path.value();
      EXPECT_EQ(total_successes_, service_->extensions()->size()) <<
          path.value();
      EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id())) <<
          path.value();
      for (std::vector<std::string>::iterator err = errors.begin();
        err != errors.end(); ++err) {
        LOG(ERROR) << *err;
      }
    } else {
      EXPECT_FALSE(installed_) << path.value();
      EXPECT_EQ(0u, loaded_.size()) << path.value();
      EXPECT_EQ(1u, errors.size()) << path.value();
    }

    installed_ = NULL;
    loaded_.clear();
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

  void UpdateExtension(const std::string& id, const FilePath& in_path,
                       bool should_succeed, bool expect_report_on_failure) {
    ASSERT_TRUE(file_util::PathExists(in_path));

    // We need to copy this to a temporary location because Update() will delete
    // it.
    FilePath path = temp_dir_.path();
    path = path.Append(in_path.BaseName());
    ASSERT_TRUE(file_util::CopyFile(in_path, path));

    service_->UpdateExtension(id, path);
    loop_.RunAllPending();
    std::vector<std::string> errors = GetErrors();

    if (should_succeed) {
      EXPECT_EQ(0u, errors.size()) << path.value();
      EXPECT_EQ(1u, service_->extensions()->size());
    } else {
      if (expect_report_on_failure) {
        EXPECT_EQ(1u, errors.size()) << path.value();
      }
    }

    // Update() should delete the temporary input file.
    EXPECT_FALSE(file_util::PathExists(path));
  }

  void ValidatePrefKeyCount(size_t count) {
    DictionaryValue* dict =
        prefs_->GetMutableDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL);
    EXPECT_EQ(count, dict->GetSize());
  }

  void ValidateBooleanPref(const std::string& extension_id,
                           const std::wstring& pref_path,
                           bool must_equal) {
    std::wstring msg = L" while checking: ";
    msg += ASCIIToWide(extension_id);
    msg += L" ";
    msg += pref_path;
    msg += L" == ";
    msg += must_equal ? L"true" : L"false";

    const DictionaryValue* dict =
        prefs_->GetDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(ASCIIToWide(extension_id), &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    bool val;
    ASSERT_TRUE(pref->GetBoolean(pref_path, &val)) << msg;
    EXPECT_EQ(must_equal, val) << msg;
  }

  bool IsPrefExist(const std::string& extension_id,
                   const std::wstring& pref_path) {
    const DictionaryValue* dict =
      prefs_->GetDictionary(L"extensions.settings");
    if (dict == NULL) return false;
    DictionaryValue* pref = NULL;
    if (!dict->GetDictionary(ASCIIToWide(extension_id), &pref)) {
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

  void ValidateIntegerPref(const std::string& extension_id,
                           const std::wstring& pref_path,
                           int must_equal) {
    std::wstring msg = L" while checking: ";
    msg += ASCIIToWide(extension_id);
    msg += L" ";
    msg += pref_path;
    msg += L" == ";
    msg += IntToWString(must_equal);

    const DictionaryValue* dict =
        prefs_->GetDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(ASCIIToWide(extension_id), &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    int val;
    ASSERT_TRUE(pref->GetInteger(pref_path, &val)) << msg;
    EXPECT_EQ(must_equal, val) << msg;
  }

  void SetPrefInteg(const std::string& extension_id,
                    const std::wstring& pref_path,
                    int value) {
    std::wstring msg = L" while setting: ";
    msg += ASCIIToWide(extension_id);
    msg += L" ";
    msg += pref_path;
    msg += L" = ";
    msg += IntToWString(value);

    const DictionaryValue* dict =
        prefs_->GetMutableDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(ASCIIToWide(extension_id), &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    pref->SetInteger(pref_path, value);
  }

 protected:
  ScopedTempDir temp_dir_;
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<Profile> profile_;
  FilePath extensions_install_dir_;
  scoped_refptr<ExtensionsService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  ExtensionList loaded_;
  std::string unloaded_id_;
  Extension* installed_;

 private:
  NotificationRegistrar registrar_;
};

FilePath::StringType NormalizeSeperators(FilePath::StringType path) {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  FilePath::StringType ret_val;
  for (size_t i = 0; i < path.length(); i++) {
    if (FilePath::IsSeparator(path[i]))
      path[i] = FilePath::kSeparators[0];
  }
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  return path;
}
// Test loading good extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectorySuccess) {
  // Initialize the test dir with a good Preferences/extensions.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");
  InitializeInstalledExtensionsService(pref_path, source_install_dir);

  service_->Init();
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(3u, loaded_.size());

  EXPECT_EQ(std::string(good0), loaded_[0]->id());
  EXPECT_EQ(std::string("My extension 1"),
            loaded_[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_[0]->description());
  EXPECT_EQ(Extension::INTERNAL, loaded_[0]->location());
  EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id()));
  EXPECT_EQ(3u, service_->extensions()->size());

  ValidatePrefKeyCount(3);
  ValidateIntegerPref(good0, L"state", Extension::ENABLED);
  ValidateIntegerPref(good0, L"location", Extension::INTERNAL);
  ValidateIntegerPref(good1, L"state", Extension::ENABLED);
  ValidateIntegerPref(good1, L"location", Extension::INTERNAL);
  ValidateIntegerPref(good2, L"state", Extension::ENABLED);
  ValidateIntegerPref(good2, L"location", Extension::INTERNAL);

  Extension* extension = loaded_[0];
  const UserScriptList& scripts = extension->content_scripts();
  const std::vector<Extension::ToolstripInfo>& toolstrips =
      extension->toolstrips();
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(3u, scripts[0].url_patterns().size());
  EXPECT_EQ("file://*",
            scripts[0].url_patterns()[0].GetAsString());
  EXPECT_EQ("http://*.google.com/*",
            scripts[0].url_patterns()[1].GetAsString());
  EXPECT_EQ("https://*.google.com/*",
            scripts[0].url_patterns()[2].GetAsString());
  EXPECT_EQ(2u, scripts[0].js_scripts().size());
  ExtensionResource resource00(scripts[0].js_scripts()[0].extension_root(),
                               scripts[0].js_scripts()[0].relative_path());
  EXPECT_TRUE(resource00.ComparePathWithDefault(
      extension->path().AppendASCII("script1.js")));
  ExtensionResource resource01(scripts[0].js_scripts()[1].extension_root(),
                               scripts[0].js_scripts()[1].relative_path());
  EXPECT_TRUE(resource01.ComparePathWithDefault(
      extension->path().AppendASCII("script2.js")));
  EXPECT_TRUE(extension->plugins().empty());
  EXPECT_EQ(1u, scripts[1].url_patterns().size());
  EXPECT_EQ("http://*.news.com/*", scripts[1].url_patterns()[0].GetAsString());
  ExtensionResource resource10(scripts[1].js_scripts()[0].extension_root(),
                               scripts[1].js_scripts()[0].relative_path());
  EXPECT_TRUE(resource10.ComparePathWithDefault(
      extension->path().AppendASCII("js_files").AppendASCII("script3.js")));
  const std::vector<URLPattern> permissions = extension->host_permissions();
  ASSERT_EQ(2u, permissions.size());
  EXPECT_EQ("http://*.google.com/*", permissions[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*", permissions[1].GetAsString());
  ASSERT_EQ(2u, toolstrips.size());
  EXPECT_EQ(extension->GetResourceURL("toolstrip1.html"),
            toolstrips[0].toolstrip);
  EXPECT_EQ(extension->GetResourceURL("lorem_ipsum.html"),
            toolstrips[0].mole);
  EXPECT_EQ(200, toolstrips[0].mole_height);
  EXPECT_EQ(extension->GetResourceURL("toolstrip2.html"),
            toolstrips[1].toolstrip);

  EXPECT_EQ(std::string(good1), loaded_[1]->id());
  EXPECT_EQ(std::string("My extension 2"), loaded_[1]->name());
  EXPECT_EQ(std::string(""), loaded_[1]->description());
  EXPECT_EQ(loaded_[1]->GetResourceURL("background.html"),
            loaded_[1]->background_url());
  EXPECT_EQ(0u, loaded_[1]->content_scripts().size());
  EXPECT_EQ(2u, loaded_[1]->plugins().size());
  EXPECT_EQ(loaded_[1]->path().AppendASCII("content_plugin.dll").value(),
            loaded_[1]->plugins()[0].path.value());
  EXPECT_TRUE(loaded_[1]->plugins()[0].is_public);
  EXPECT_EQ(loaded_[1]->path().AppendASCII("extension_plugin.dll").value(),
            loaded_[1]->plugins()[1].path.value());
  EXPECT_FALSE(loaded_[1]->plugins()[1].is_public);
  EXPECT_EQ(Extension::INTERNAL, loaded_[1]->location());

  EXPECT_EQ(std::string(good2), loaded_[2]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_[2]->name());
  EXPECT_EQ(std::string(""), loaded_[2]->description());
  EXPECT_EQ(1u, loaded_[2]->content_scripts().size());
  EXPECT_EQ(Extension::INTERNAL, loaded_[2]->location());
};

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectoryFail) {
  // Initialize the test dir with a good Preferences/extensions.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("bad")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");

  InitializeInstalledExtensionsService(pref_path, source_install_dir);

  service_->Init();
  loop_.RunAllPending();

  ASSERT_EQ(4u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());

  EXPECT_TRUE(MatchPattern(GetErrors()[0],
      std::string("Could not load extension from '*'. * ") +
      base::JSONReader::kBadRootElementType)) << GetErrors()[0];

  EXPECT_TRUE(MatchPattern(GetErrors()[1],
      std::string("Could not load extension from '*'. ") +
      extension_manifest_errors::kManifestUnreadable)) << GetErrors()[1];

  EXPECT_TRUE(MatchPattern(GetErrors()[2],
      std::string("Could not load extension from '*'. ") +
      extension_manifest_errors::kMissingFile)) << GetErrors()[2];

  EXPECT_TRUE(MatchPattern(GetErrors()[3],
      std::string("Could not load extension from '*'. ") +
      extension_manifest_errors::kManifestUnreadable)) << GetErrors()[3];
};

// Test that partially deleted extensions are cleaned up during startup
// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, CleanupOnStartup) {
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");

  InitializeInstalledExtensionsService(pref_path, source_install_dir);

  // Simulate that one of them got partially deleted by clearing its pref.
  DictionaryValue* dict = prefs_->GetMutableDictionary(L"extensions.settings");
  ASSERT_TRUE(dict != NULL);
  dict->Remove(L"behllobkkfkfnphdnhnkndlbkcpglgmj", NULL);

  service_->Init();
  loop_.RunAllPending();

  file_util::FileEnumerator dirs(extensions_install_dir_, false,
                                 file_util::FileEnumerator::DIRECTORIES);
  size_t count = 0;
  while (!dirs.Next().empty())
    count++;

  // We should have only gotten two extensions now.
  EXPECT_EQ(2u, count);

  // And extension1 dir should now be toast.
  FilePath extension_dir = extensions_install_dir_
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj");
  ASSERT_FALSE(file_util::PathExists(extension_dir));
}

// Test installing extensions. This test tries to install few extensions using
// crx files. If you need to change those crx files, feel free to repackage
// them, throw away the key used and change the id's above.
TEST_F(ExtensionsServiceTest, InstallExtension) {
  InitializeEmptyExtensionsService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // Extensions not enabled.
  set_extensions_enabled(false);
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, false);
  set_extensions_enabled(true);

  ValidatePrefKeyCount(0);

  // A simple extension that should install without error.
  path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);
  // TODO(erikkay): verify the contents of the installed extension.

  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::INTERNAL);

  // An extension with page actions.
  path = extensions_path.AppendASCII("page_action.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(page_action, L"state", Extension::ENABLED);
  ValidateIntegerPref(page_action, L"location", Extension::INTERNAL);

  // Bad signature.
  path = extensions_path.AppendASCII("bad_signature.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // 0-length extension file.
  path = extensions_path.AppendASCII("not_an_extension.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // Bad magic number.
  path = extensions_path.AppendASCII("bad_magic.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // Extensions cannot have folders or files that have underscores except ofr in
  // certain whitelisted cases (eg _locales). This is an example of a broader
  // class of validation that we do to the directory structure of the extension.
  // We did not used to handle this correctly for installation.
  path = extensions_path.AppendASCII("bad_underscore.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

// Test Packaging and installing an extension.
// TODO(rafaelw): add more tests for failure cases.
TEST_F(ExtensionsServiceTest, PackExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  FilePath output_directory;
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_"),
      &output_directory);
  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, FilePath(),
      privkey_path));

  ASSERT_TRUE(file_util::PathExists(privkey_path));
  InstallExtension(crx_path, true);

  file_util::Delete(crx_path, false);
  file_util::Delete(privkey_path, false);
}

// Test Packaging and installing an extension using an openssl generated key.
// The openssl is generated with the following:
// > openssl genrsa -out privkey.pem 1024
// > openssl pkcs8 -topk8 -nocrypt -in privkey.pem -out privkey_asn1.pem
// The privkey.pem is a PrivateKey, and the pcks8 -topk8 creates a
// PrivateKeyInfo ASN.1 structure, we our RSAPrivateKey expects.
TEST_F(ExtensionsServiceTest, PackExtensionOpenSSLKey) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");
  FilePath privkey_path(extensions_path.AppendASCII(
      "openssl_privkey_asn1.pem"));
  ASSERT_TRUE(file_util::PathExists(privkey_path));

  FilePath output_directory;
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_"),
      &output_directory);
  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, privkey_path,
      FilePath()));

  InstallExtension(crx_path, true);

  file_util::Delete(crx_path, false);
}

TEST_F(ExtensionsServiceTest, InstallTheme) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A theme.
  FilePath path = extensions_path.AppendASCII("theme.crx");
  InstallExtension(path, true);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(theme_crx, L"location", Extension::INTERNAL);

  // A theme when extensions are disabled. Themes can be installed, even when
  // extensions are disabled.
  set_extensions_enabled(false);
  path = extensions_path.AppendASCII("theme2.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme2_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(theme2_crx, L"location", Extension::INTERNAL);

  // A theme with extension elements. Themes cannot have extension elements so
  // this test should fail.
  set_extensions_enabled(true);
  path = extensions_path.AppendASCII("theme_with_extension.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // A theme with image resources missing (misspelt path).
  path = extensions_path.AppendASCII("theme_missing_image.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);
}

// Test that when an extension version is reinstalled, nothing happens.
TEST_F(ExtensionsServiceTest, Reinstall) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::INTERNAL);

  installed_ = NULL;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();

  // Reinstall the same version, nothing should happen.
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::INTERNAL);
}

// Test upgrading a signed extension.
TEST_F(ExtensionsServiceTest, UpgradeSignedGood) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ASSERT_EQ(0u, GetErrors().size());

  // Upgrade to version 2.0
  path = extensions_path.AppendASCII("good2.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ASSERT_EQ(0u, GetErrors().size());
}

// Test upgrading a signed extension with a bad signature.
TEST_F(ExtensionsServiceTest, UpgradeSignedBad) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  installed_ = NULL;

  // Try upgrading with a bad signature. This should fail during the unpack,
  // because the key will not match the signature.
  path = extensions_path.AppendASCII("good2_bad_signature.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_FALSE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(1u, GetErrors().size());
}

// Test a normal update via the UpdateExtension API
TEST_F(ExtensionsServiceTest, UpdateExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  Extension* good = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  path = extensions_path.AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, true, true);
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
}

// Test updating a not-already-installed extension - this should fail
TEST_F(ExtensionsServiceTest, UpdateNotInstalledExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->UpdateExtension(good_crx, path);
  loop_.RunAllPending();

  ASSERT_EQ(0u, service_->extensions()->size());
  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
}

// Makes sure you can't downgrade an extension via UpdateExtension
TEST_F(ExtensionsServiceTest, UpdateWillNotDowngrade) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good2.crx");

  InstallExtension(path, true);
  Extension* good = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.1", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Change path from good2.crx -> good.crx
  path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(good_crx, path, false, true);
  ASSERT_EQ("1.0.0.1", service_->extensions()->at(0)->VersionString());
}

// Make sure calling update with an identical version does nothing
TEST_F(ExtensionsServiceTest, UpdateToSameVersionIsNoop) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  Extension* good = service_->extensions()->at(0);
  ASSERT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, false, false);
}

// Test pref settings for blacklist and unblacklist extensions.
TEST_F(ExtensionsServiceTest, SetUnsetBlacklistInPrefs) {
  InitializeEmptyExtensionsService();
  std::vector<std::string> blacklist;
  blacklist.push_back(good0);
  blacklist.push_back("invalid_id");  // an invalid id
  blacklist.push_back(good1);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // blacklist is set for good0,1,2
  ValidateBooleanPref(good0, L"blacklist", true);
  ValidateBooleanPref(good1, L"blacklist", true);
  // invalid_id should not be inserted to pref.
  EXPECT_FALSE(IsPrefExist("invalid_id", L"blacklist"));

  // remove good1, add good2
  blacklist.pop_back();
  blacklist.push_back(good2);

  service_->UpdateExtensionBlacklist(blacklist);
  // only good0 and good1 should be set
  ValidateBooleanPref(good0, L"blacklist", true);
  EXPECT_FALSE(IsPrefExist(good1, L"blacklist"));
  ValidateBooleanPref(good2, L"blacklist", true);
  EXPECT_FALSE(IsPrefExist("invalid_id", L"blacklist"));
}

// Unload installed extension from blacklist.
TEST_F(ExtensionsServiceTest, UnloadBlacklistedExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  Extension* good = service_->extensions()->at(0);
  EXPECT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, false, false);

  std::vector<std::string> blacklist;
  blacklist.push_back(good_crx);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // Now, the good_crx is blacklisted.
  ValidateBooleanPref(good_crx, L"blacklist", true);
  EXPECT_EQ(0u, service_->extensions()->size());

  // Remove good_crx from blacklist
  blacklist.pop_back();
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();
  // blacklist value should not be set for good_crx
  EXPECT_FALSE(IsPrefExist(good_crx, L"blacklist"));
}

// Unload installed extension from blacklist.
TEST_F(ExtensionsServiceTest, BlacklistedExtensionWillNotInstall) {
  InitializeEmptyExtensionsService();
  std::vector<std::string> blacklist;
  blacklist.push_back(good_crx);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // Now, the good_crx is blacklisted.
  ValidateBooleanPref(good_crx, L"blacklist", true);

  // We can not install good_crx.
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();
  EXPECT_EQ(0u, service_->extensions()->size());
  ValidateBooleanPref(good_crx, L"blacklist", true);
}

// Test loading extensions from the profile directory, except
// blacklisted ones.
TEST_F(ExtensionsServiceTest, WillNotLoadBlacklistedExtensionsFromDirectory) {
  // Initialize the test dir with a good Preferences/extensions.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");
  InitializeInstalledExtensionsService(pref_path, source_install_dir);

  // Blacklist good0.
  std::vector<std::string> blacklist;
  blacklist.push_back(good0);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  ValidateBooleanPref(good0, L"blacklist", true);

  // Load extensions.
  service_->Init();
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  EXPECT_EQ(2u, loaded_.size());

  EXPECT_NE(std::string(good0), loaded_[0]->id());
  EXPECT_NE(std::string(good0), loaded_[1]->id());
}

// Tests uninstalling normal extensions
TEST_F(ExtensionsServiceTest, UninstallExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);

  // The directory should be there now.
  const char* extension_id = good_crx;
  FilePath extension_path = extensions_install_dir_.AppendASCII(extension_id);
  EXPECT_TRUE(file_util::PathExists(extension_path));

  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::INTERNAL);

  // Uninstall it.
  service_->UninstallExtension(extension_id, false);
  total_successes_ = 0;

  // We should get an unload notification.
  ASSERT_TRUE(unloaded_id_.length());
  EXPECT_EQ(extension_id, unloaded_id_);

  ValidatePrefKeyCount(0);

  // The extension should not be in the service anymore.
  ASSERT_FALSE(service_->GetExtensionById(extension_id));
  loop_.RunAllPending();

  // The directory should be gone.
  EXPECT_FALSE(file_util::PathExists(extension_path));
}

// Tests loading single extensions (like --load-extension)
TEST_F(ExtensionsServiceTest, LoadExtension) {
  InitializeEmptyExtensionsService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath ext1 = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");
  service_->LoadExtension(ext1);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Extension::LOAD, loaded_[0]->location());
  EXPECT_EQ(1u, service_->extensions()->size());

  // --load-extension doesn't add entries to prefs
  ValidatePrefKeyCount(0);

  FilePath no_manifest = extensions_path
      .AppendASCII("bad")
      // .AppendASCII("Extensions")
      .AppendASCII("cccccccccccccccccccccccccccccccc")
      .AppendASCII("1");
  service_->LoadExtension(no_manifest);
  loop_.RunAllPending();
  EXPECT_EQ(1u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, service_->extensions()->size());

  // Test uninstall.
  std::string id = loaded_[0]->id();
  EXPECT_FALSE(unloaded_id_.length());
  service_->UninstallExtension(id, false);
  loop_.RunAllPending();
  EXPECT_EQ(id, unloaded_id_);
  ASSERT_EQ(0u, loaded_.size());
  EXPECT_EQ(0u, service_->extensions()->size());
}

// Tests that we generate IDs when they are not specified in the manifest for
// --load-extension.
TEST_F(ExtensionsServiceTest, GenerateID) {
  InitializeEmptyExtensionsService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath no_id_ext = extensions_path.AppendASCII("no_id");
  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_TRUE(Extension::IdIsValid(loaded_[0]->id()));
  EXPECT_EQ(loaded_[0]->location(), Extension::LOAD);

  // --load-extension doesn't add entries to prefs
  ValidatePrefKeyCount(0);

  std::string previous_id = loaded_[0]->id();

  // If we reload the same path, we should get the same extension ID.
  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(previous_id, loaded_[0]->id());
}

// Tests the external installation feature
#if defined(OS_WIN)

TEST_F(ExtensionsServiceTest, ExternalInstallRegistry) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionsService();
  set_extensions_enabled(false);
  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* reg_provider =
      new MockExtensionProvider(Extension::EXTERNAL_REGISTRY);
  SetMockExternalProvider(Extension::EXTERNAL_REGISTRY, reg_provider);

  // Register a test extension externally using the mock registry provider.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  // Add the extension.
  reg_provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Reloading extensions should find our externally registered extension
  // and install it.
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL_REGISTRY, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Reload extensions without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  reg_provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Uninstall the extension and reload. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_[0]->id();
  service_->UninstallExtension(id, false);
  loop_.RunAllPending();

  // The extension should also be gone from the install directory.
  FilePath install_path = extensions_install_dir_.AppendASCII(id);
  ASSERT_FALSE(file_util::PathExists(install_path));

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::KILLBIT);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Now clear the preference, reinstall, then remove the reg key. The extension
  // should be uninstalled.
  SetPrefInteg(good_crx, L"state", Extension::ENABLED);
  prefs_->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Now test an externally triggered uninstall (deleting the registry key).
  reg_provider->RemoveExtension(good_crx);

  loaded_.clear();
  service_->LoadAllExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(0);

  // The extension should also be gone from the install directory.
  ASSERT_FALSE(file_util::PathExists(install_path));
}

#endif

TEST_F(ExtensionsServiceTest, ExternalInstallPref) {
  InitializeEmptyExtensionsService();
  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(Extension::EXTERNAL_PREF);
  SetMockExternalProvider(Extension::EXTERNAL_PREF, pref_provider);

  // Register a external extension using preinstalled preferences.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  // Add the extension.
  pref_provider->UpdateOrAddExtension(good_crx, "1.0", source_path);

  // Checking for updates should find our externally registered extension
  // and install it.
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL_PREF, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Reload extensions without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  pref_provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Uninstall the extension and reload. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_[0]->id();
  service_->UninstallExtension(id, false);
  loop_.RunAllPending();

  // The extension should also be gone from the install directory.
  FilePath install_path = extensions_install_dir_.AppendASCII(id);
  ASSERT_FALSE(file_util::PathExists(install_path));

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::KILLBIT);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now clear the preference and reinstall.
  SetPrefInteg(good_crx, L"state", Extension::ENABLED);
  prefs_->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, L"state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now test an externally triggered uninstall (deleting id from json file).
  pref_provider->RemoveExtension(good_crx);

  loaded_.clear();
  service_->LoadAllExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(0);

  // The extension should also be gone from the install directory.
  ASSERT_FALSE(file_util::PathExists(install_path));

  // It should still work if extensions are disabled (disableness doesn't
  // apply to externally registered extensions).
  set_extensions_enabled(false);

  pref_provider->UpdateOrAddExtension(good_crx, "1.0", source_path);
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(1u, GetErrors().size());
}

TEST_F(ExtensionsServiceTest, ExternalPrefProvider) {
  InitializeEmptyExtensionsService();
  std::string json_data =
      "{"
        "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
          "\"external_crx\": \"RandomExtension.crx\","
          "\"external_version\": \"1.0\""
        "},"
        "\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
          "\"external_crx\": \"RandomExtension2.crx\","
          "\"external_version\": \"2.0\""
        "}"
      "}";

  MockProviderVisitor visitor;
  std::set<std::string> ignore_list;
  EXPECT_EQ(2, visitor.Visit(json_data, ignore_list));
  ignore_list.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_EQ(1, visitor.Visit(json_data, ignore_list));
  ignore_list.insert("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  EXPECT_EQ(0, visitor.Visit(json_data, ignore_list));

  // Use a json that contains three invalid extensions:
  // - One that is missing the 'external_crx' key.
  // - One that is missing the 'external_version' key.
  // - One that is specifying .. in the path.
  // - Plus one valid extension to make sure the json file is parsed properly.
  json_data =
      "{"
        "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
          "\"external_version\": \"1.0\""
        "},"
        "\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
          "\"external_crx\": \"RandomExtension.crx\""
        "},"
        "\"cccccccccccccccccccccccccccccccc\": {"
          "\"external_crx\": \"..\\\\foo\\\\RandomExtension2.crx\","
          "\"external_version\": \"2.0\""
        "},"
        "\"dddddddddddddddddddddddddddddddddd\": {"
          "\"external_crx\": \"RandomValidExtension.crx\","
          "\"external_version\": \"1.0\""
        "}"
      "}";
  ignore_list.clear();
  EXPECT_EQ(1, visitor.Visit(json_data, ignore_list));
}

class ExtensionsReadyRecorder : public NotificationObserver {
 public:
  ExtensionsReadyRecorder() : ready_(false) {
    registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                   NotificationService::AllSources());
  }

  void set_ready(bool value) { ready_ = value; }
  bool ready() { return ready_; }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSIONS_READY:
        ready_ = true;
        break;
      default:
        NOTREACHED();
    }
  }

  NotificationRegistrar registrar_;
  bool ready_;
};

// Test that we get enabled/disabled correctly for all the pref/command-line
// combinations. We don't want to derive from the ExtensionsServiceTest class
// for this test, so we use ExtensionsServiceTestSimple.
//
// Also tests that we always fire EXTENSIONS_READY, no matter whether we are
// enabled or not.
TEST(ExtensionsServiceTestSimple, Enabledness) {
  ExtensionsReadyRecorder recorder;
  TestingProfile profile;
  MessageLoop loop;
  scoped_ptr<CommandLine> command_line;
  scoped_refptr<ExtensionsService> service;
  FilePath install_dir = profile.GetPath()
      .AppendASCII(ExtensionsService::kInstallDirectoryName);

  // By default, we are enabled.
  command_line.reset(new CommandLine(CommandLine::ARGUMENTS_ONLY));
  service = new ExtensionsService(&profile, command_line.get(),
      profile.GetPrefs(), install_dir, &loop, &loop, false);
  EXPECT_TRUE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  // If either the command line or pref is set, we are disabled.
  recorder.set_ready(false);
  command_line->AppendSwitch(switches::kDisableExtensions);
  service = new ExtensionsService(&profile, command_line.get(),
      profile.GetPrefs(), install_dir, &loop, &loop, false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  profile.GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
  service = new ExtensionsService(&profile, command_line.get(),
      profile.GetPrefs(), install_dir, &loop, &loop, false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  command_line.reset(new CommandLine(CommandLine::ARGUMENTS_ONLY));
  service = new ExtensionsService(&profile, command_line.get(),
      profile.GetPrefs(), install_dir, &loop, &loop, false);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());
}
