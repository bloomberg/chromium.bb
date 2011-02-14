// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_unittest.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/appcache/chrome_appcache_service.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_extension_provider_interface.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"
#include "chrome/browser/extensions/external_pref_extension_loader.h"
#include "chrome/browser/extensions/pack_extension_job.cc"
#include "chrome/browser/file_system/browser_file_system_helper.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_options.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"

namespace keys = extension_manifest_keys;

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
const char* const permissions_crx = "eagpmdpfmaekmmcejjbmjoecnejeiiin";

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

static void AddPattern(ExtensionExtent* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

static void AssertEqualExtents(ExtensionExtent* extent1,
                               ExtensionExtent* extent2) {
  std::vector<URLPattern> patterns1 = extent1->patterns();
  std::vector<URLPattern> patterns2 = extent2->patterns();
  std::set<std::string> strings1;
  EXPECT_EQ(patterns1.size(), patterns2.size());

  for (size_t i = 0; i < patterns1.size(); ++i)
    strings1.insert(patterns1.at(i).GetAsString());

  std::set<std::string> strings2;
  for (size_t i = 0; i < patterns2.size(); ++i)
    strings2.insert(patterns2.at(i).GetAsString());

  EXPECT_EQ(strings1, strings2);
}

}  // namespace

class MockExtensionProvider : public ExternalExtensionProviderInterface {
 public:
  explicit MockExtensionProvider(
      VisitorInterface* visitor,
      Extension::Location location)
  : location_(location), visitor_(visitor), visit_count_(0) {
  }
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
  virtual void VisitRegisteredExtension() const {
    visit_count_++;
    for (DataMap::const_iterator i = extension_map_.begin();
         i != extension_map_.end(); ++i) {
      scoped_ptr<Version> version;
      version.reset(Version::GetVersionFromString(i->second.first));

      visitor_->OnExternalExtensionFileFound(
          i->first, version.get(), i->second.second, location_);
    }
    visitor_->OnExternalProviderReady();
  }

  virtual bool HasExtension(const std::string& id) const {
    return extension_map_.find(id) != extension_map_.end();
  }

  virtual bool GetExtensionDetails(const std::string& id,
                                   Extension::Location* location,
                                   scoped_ptr<Version>* version) const {
    DataMap::const_iterator it = extension_map_.find(id);
    if (it == extension_map_.end())
      return false;

    if (version)
      version->reset(Version::GetVersionFromString(it->second.first));

    if (location)
      *location = location_;

    return true;
  }

  virtual bool IsReady() {
    return true;
  }

  virtual void ServiceShutdown() {
  }

  int visit_count() const { return visit_count_; }
  void set_visit_count(int visit_count) {
    visit_count_ = visit_count;
  }

 private:
  typedef std::map< std::string, std::pair<std::string, FilePath> > DataMap;
  DataMap extension_map_;
  Extension::Location location_;
  VisitorInterface* visitor_;

  // visit_count_ tracks the number of calls to VisitRegisteredExtension().
  // Mutable because it must be incremented on each call to
  // VisitRegisteredExtension(), which must be a const method to inherit
  // from the class being mocked.
  mutable int visit_count_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionProvider);
};

class MockProviderVisitor
    : public ExternalExtensionProviderInterface::VisitorInterface {
 public:

  // The provider will return |fake_base_path| from
  // GetBaseCrxFilePath().  User can test the behavior with
  // and without an empty path using this parameter.
  explicit MockProviderVisitor(FilePath fake_base_path)
      : ids_found_(0),
        fake_base_path_(fake_base_path) {
  }

  int Visit(const std::string& json_data) {
    // Give the test json file to the provider for parsing.
    provider_.reset(new ExternalExtensionProviderImpl(
        this,
        new ExternalTestingExtensionLoader(json_data, fake_base_path_),
        Extension::EXTERNAL_PREF,
        Extension::EXTERNAL_PREF_DOWNLOAD));

    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    JSONStringValueSerializer serializer(json_data);
    Value* json_value = serializer.Deserialize(NULL, NULL);

    if (!json_value || !json_value->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Unable to deserialize json data";
      return -1;
    } else {
      DictionaryValue* external_extensions =
          static_cast<DictionaryValue*>(json_value);
      prefs_.reset(external_extensions);
    }

    // Reset our counter.
    ids_found_ = 0;
    // Ask the provider to look up all extensions and return them.
    provider_->VisitRegisteredExtension();

    return ids_found_;
  }

  virtual void OnExternalExtensionFileFound(const std::string& id,
                                            const Version* version,
                                            const FilePath& path,
                                            Extension::Location unused) {
    ++ids_found_;
    DictionaryValue* pref;
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(prefs_->GetDictionary(id, &pref))
       << "Got back ID (" << id.c_str() << ") we weren't expecting";

    EXPECT_TRUE(path.IsAbsolute());
    if (!fake_base_path_.empty())
      EXPECT_TRUE(fake_base_path_.IsParent(path));

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(id));

      // Ask provider if the extension we got back is registered.
      Extension::Location location = Extension::INVALID;
      scoped_ptr<Version> v1;
      FilePath crx_path;

      EXPECT_TRUE(provider_->GetExtensionDetails(id, NULL, &v1));
      EXPECT_STREQ(version->GetString().c_str(), v1->GetString().c_str());

      scoped_ptr<Version> v2;
      EXPECT_TRUE(provider_->GetExtensionDetails(id, &location, &v2));
      EXPECT_STREQ(version->GetString().c_str(), v1->GetString().c_str());
      EXPECT_STREQ(version->GetString().c_str(), v2->GetString().c_str());
      EXPECT_EQ(Extension::EXTERNAL_PREF, location);

      // Remove it so we won't count it ever again.
      prefs_->Remove(id, NULL);
    }
  }

  virtual void OnExternalExtensionUpdateUrlFound(
      const std::string& id, const GURL& update_url,
      Extension::Location location) {
    ++ids_found_;
    DictionaryValue* pref;
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(prefs_->GetDictionary(id, &pref))
       << L"Got back ID (" << id.c_str() << ") we weren't expecting";
    EXPECT_EQ(Extension::EXTERNAL_PREF_DOWNLOAD, location);

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(id));

      // External extensions with update URLs do not have versions.
      scoped_ptr<Version> v1;
      Extension::Location location1 = Extension::INVALID;
      EXPECT_TRUE(provider_->GetExtensionDetails(id, &location1, &v1));
      EXPECT_FALSE(v1.get());
      EXPECT_EQ(Extension::EXTERNAL_PREF_DOWNLOAD, location1);

      // Remove it so we won't count it again.
      prefs_->Remove(id, NULL);
    }
  }

  virtual void OnExternalProviderReady() {
    EXPECT_TRUE(provider_->IsReady());
  }

 private:
  int ids_found_;
  FilePath fake_base_path_;
  scoped_ptr<ExternalExtensionProviderImpl> provider_;
  scoped_ptr<DictionaryValue> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockProviderVisitor);
};

class ExtensionTestingProfile : public TestingProfile {
 public:
  ExtensionTestingProfile() : service_(NULL) {
  }

  void set_extensions_service(ExtensionService* service) {
    service_ = service;
  }
  virtual ExtensionService* GetExtensionService() { return service_; }

  virtual ChromeAppCacheService* GetAppCacheService() {
    if (!appcache_service_) {
      appcache_service_ = new ChromeAppCacheService;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(appcache_service_.get(),
                            &ChromeAppCacheService::InitializeOnIOThread,
                            GetPath(), IsOffTheRecord(),
                            make_scoped_refptr(GetHostContentSettingsMap()),
                            false));
    }
    return appcache_service_;
  }

  virtual fileapi::FileSystemContext* GetFileSystemContext() {
    if (!file_system_context_)
      file_system_context_ = CreateFileSystemContext(
          GetPath(), IsOffTheRecord());
    return file_system_context_;
  }

 private:
  ExtensionService* service_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
};

// Our message loop may be used in tests which require it to be an IO loop.
ExtensionServiceTestBase::ExtensionServiceTestBase()
    : total_successes_(0),
      loop_(MessageLoop::TYPE_IO),
      ui_thread_(BrowserThread::UI, &loop_),
      db_thread_(BrowserThread::DB, &loop_),
      webkit_thread_(BrowserThread::WEBKIT, &loop_),
      file_thread_(BrowserThread::FILE, &loop_),
      io_thread_(BrowserThread::IO, &loop_) {
}

ExtensionServiceTestBase::~ExtensionServiceTestBase() {
  // Drop our reference to ExtensionService and TestingProfile, so that they
  // can be destroyed while BrowserThreads and MessageLoop are still around
  // (they are used in the destruction process).
  service_ = NULL;
  profile_.reset(NULL);
  MessageLoop::current()->RunAllPending();
}

void ExtensionServiceTestBase::InitializeExtensionService(
    const FilePath& pref_file, const FilePath& extensions_install_dir) {
  ExtensionTestingProfile* profile = new ExtensionTestingProfile();
  // Create a PrefService that only contains user defined preference values.
  PrefService* prefs =
      PrefServiceMockBuilder().WithUserFilePrefs(pref_file).Create();
  Profile::RegisterUserPrefs(prefs);
  browser::RegisterUserPrefs(prefs);
  profile->SetPrefService(prefs);

  profile_.reset(profile);

  service_ = profile->CreateExtensionService(
      CommandLine::ForCurrentProcess(),
      extensions_install_dir);
  service_->set_extensions_enabled(true);
  service_->set_show_extensions_prompts(false);
  profile->set_extensions_service(service_.get());

  // When we start up, we want to make sure there is no external provider,
  // since the ExtensionService on Windows will use the Registry as a default
  // provider and if there is something already registered there then it will
  // interfere with the tests. Those tests that need an external provider
  // will register one specifically.
  service_->ClearProvidersForTesting();

  total_successes_ = 0;
}

void ExtensionServiceTestBase::InitializeInstalledExtensionService(
    const FilePath& prefs_file, const FilePath& source_install_dir) {
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

  InitializeExtensionService(temp_prefs, extensions_install_dir_);
}

void ExtensionServiceTestBase::InitializeEmptyExtensionService() {
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

  InitializeExtensionService(prefs_filename, extensions_install_dir_);
}

// static
void ExtensionServiceTestBase::SetUpTestCase() {
  ExtensionErrorReporter::Init(false);  // no noisy errors
}

void ExtensionServiceTestBase::SetUp() {
  ExtensionErrorReporter::GetInstance()->ClearErrors();
}

class ExtensionServiceTest
  : public ExtensionServiceTestBase, public NotificationObserver {
 public:
  ExtensionServiceTest() : installed_(NULL) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::THEME_INSTALLED,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED: {
        const Extension* extension = Details<const Extension>(details).ptr();
        loaded_.push_back(make_scoped_refptr(extension));
        // The tests rely on the errors being in a certain order, which can vary
        // depending on how filesystem iteration works.
        std::stable_sort(loaded_.begin(), loaded_.end(), ExtensionsOrder());
        break;
      }

      case NotificationType::EXTENSION_UNLOADED: {
        const Extension* e =
            Details<UnloadedExtensionInfo>(details)->extension;
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
        installed_ = Details<const Extension>(details).ptr();
        break;

      default:
        DCHECK(false);
    }
  }

  void AddMockExternalProvider(ExternalExtensionProviderInterface* provider) {
    service_->AddProviderForTesting(provider);
  }

 protected:
  void TestExternalProvider(MockExtensionProvider* provider,
                            Extension::Location location);

  void PackAndInstallExtension(const FilePath& dir_path,
                               const FilePath& pem_path,
                               bool should_succeed) {
    FilePath crx_path;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &crx_path));
    crx_path = crx_path.AppendASCII("temp.crx");

    // Use the existing pem key, if provided.
    FilePath pem_output_path;
    if (pem_path.value().empty()) {
      pem_output_path = crx_path.DirName().AppendASCII("temp.pem");
      ASSERT_TRUE(file_util::Delete(pem_output_path, false));
    } else {
      ASSERT_TRUE(file_util::PathExists(pem_path));
    }

    ASSERT_TRUE(file_util::Delete(crx_path, false));

    scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
    ASSERT_TRUE(creator->Run(dir_path,
                             crx_path,
                             pem_path,
                             pem_output_path));

    ASSERT_TRUE(file_util::PathExists(crx_path));

    InstallExtension(crx_path, should_succeed);
  }

  void PackAndInstallExtension(const FilePath& dir_path,
                               bool should_succeed) {
      PackAndInstallExtension(dir_path, FilePath(), should_succeed);
  }

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
      EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id(), false)) <<
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

  enum UpdateState {
    FAILED_SILENTLY,
    FAILED,
    UPDATED,
    INSTALLED,
    ENABLED
  };

  void UpdateExtension(const std::string& id, const FilePath& in_path,
                       UpdateState expected_state) {
    ASSERT_TRUE(file_util::PathExists(in_path));

    // We need to copy this to a temporary location because Update() will delete
    // it.
    FilePath path = temp_dir_.path();
    path = path.Append(in_path.BaseName());
    ASSERT_TRUE(file_util::CopyFile(in_path, path));

    int previous_enabled_extension_count =
        service_->extensions()->size();
    int previous_installed_extension_count =
        previous_enabled_extension_count +
        service_->disabled_extensions()->size();

    service_->UpdateExtension(id, path, GURL());
    loop_.RunAllPending();

    std::vector<std::string> errors = GetErrors();
    int error_count = errors.size();
    int enabled_extension_count =
        service_->extensions()->size();
    int installed_extension_count =
        enabled_extension_count + service_->disabled_extensions()->size();

    int expected_error_count = (expected_state == FAILED) ? 1 : 0;
    EXPECT_EQ(expected_error_count, error_count) << path.value();

    if (expected_state <= FAILED) {
      EXPECT_EQ(previous_enabled_extension_count,
                enabled_extension_count);
      EXPECT_EQ(previous_installed_extension_count,
                installed_extension_count);
    } else {
      int expected_installed_extension_count =
          (expected_state >= INSTALLED) ? 1 : 0;
      int expected_enabled_extension_count =
          (expected_state >= ENABLED) ? 1 : 0;
      EXPECT_EQ(expected_installed_extension_count,
                installed_extension_count);
      EXPECT_EQ(expected_enabled_extension_count,
                enabled_extension_count);
    }

    // Update() should delete the temporary input file.
    EXPECT_FALSE(file_util::PathExists(path));
  }

  void ValidatePrefKeyCount(size_t count) {
    DictionaryValue* dict =
        profile_->GetPrefs()->GetMutableDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL);
    EXPECT_EQ(count, dict->size());
  }

  void ValidateBooleanPref(const std::string& extension_id,
                           const std::string& pref_path,
                           bool expected_val) {
    std::string msg = " while checking: ";
    msg += extension_id;
    msg += " ";
    msg += pref_path;
    msg += " == ";
    msg += expected_val ? "true" : "false";

    PrefService* prefs = profile_->GetPrefs();
    const DictionaryValue* dict =
        prefs->GetDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    bool val;
    ASSERT_TRUE(pref->GetBoolean(pref_path, &val)) << msg;
    EXPECT_EQ(expected_val, val) << msg;
  }

  bool IsPrefExist(const std::string& extension_id,
                   const std::string& pref_path) {
    const DictionaryValue* dict =
        profile_->GetPrefs()->GetDictionary("extensions.settings");
    if (dict == NULL) return false;
    DictionaryValue* pref = NULL;
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

  void ValidateIntegerPref(const std::string& extension_id,
                           const std::string& pref_path,
                           int expected_val) {
    std::string msg = " while checking: ";
    msg += extension_id;
    msg += " ";
    msg += pref_path;
    msg += " == ";
    msg += base::IntToString(expected_val);

    PrefService* prefs = profile_->GetPrefs();
    const DictionaryValue* dict =
        prefs->GetDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    int val;
    ASSERT_TRUE(pref->GetInteger(pref_path, &val)) << msg;
    EXPECT_EQ(expected_val, val) << msg;
  }

  void ValidateStringPref(const std::string& extension_id,
                          const std::string& pref_path,
                          const std::string& expected_val) {
    std::string msg = " while checking: ";
    msg += extension_id;
    msg += ".manifest.";
    msg += pref_path;
    msg += " == ";
    msg += expected_val;

    const DictionaryValue* dict =
        profile_->GetPrefs()->GetDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    std::string manifest_path = extension_id + ".manifest";
    ASSERT_TRUE(dict->GetDictionary(manifest_path, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    std::string val;
    ASSERT_TRUE(pref->GetString(pref_path, &val)) << msg;
    EXPECT_EQ(expected_val, val) << msg;
  }

  void SetPref(const std::string& extension_id,
               const std::string& pref_path,
               Value* value,
               const std::string& msg) {
    const DictionaryValue* dict =
        profile_->GetPrefs()->GetMutableDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
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

    SetPref(extension_id, pref_path, Value::CreateIntegerValue(value), msg);
  }

  void SetPrefBool(const std::string& extension_id,
                   const std::string& pref_path,
                   bool value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;
    msg += " = ";
    msg += (value ? "true" : "false");

    SetPref(extension_id, pref_path, Value::CreateBooleanValue(value), msg);
  }

  void ClearPref(const std::string& extension_id,
                 const std::string& pref_path) {
    std::string msg = " while clearing: ";
    msg += extension_id + " " + pref_path;

    const DictionaryValue* dict =
        profile_->GetPrefs()->GetMutableDictionary("extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    pref->Remove(pref_path, NULL);
  }

  void SetPrefStringSet(const std::string& extension_id,
                        const std::string& pref_path,
                        const std::set<std::string>& value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;

    ListValue* list_value = new ListValue();
    for (std::set<std::string>::const_iterator iter = value.begin();
         iter != value.end(); ++iter)
      list_value->Append(Value::CreateStringValue(*iter));

    SetPref(extension_id, pref_path, list_value, msg);
  }

 protected:
  ExtensionList loaded_;
  std::string unloaded_id_;
  const Extension* installed_;

 private:
  NotificationRegistrar registrar_;
};

FilePath NormalizeSeparators(const FilePath& path) {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  return path.NormalizeWindowsPathSeparators();
#else
  return path;
#endif  // FILE_PATH_USES_WIN_SEPARATORS
}

// Receives notifications from a PackExtensionJob, indicating either that
// packing succeeded or that there was some error.
class PackExtensionTestClient : public PackExtensionJob::Client {
 public:
  PackExtensionTestClient(const FilePath& expected_crx_path,
                          const FilePath& expected_private_key_path);
  virtual void OnPackSuccess(const FilePath& crx_path,
                             const FilePath& private_key_path);
  virtual void OnPackFailure(const std::string& error_message);

 private:
  const FilePath expected_crx_path_;
  const FilePath expected_private_key_path_;
  DISALLOW_COPY_AND_ASSIGN(PackExtensionTestClient);
};

PackExtensionTestClient::PackExtensionTestClient(
    const FilePath& expected_crx_path,
    const FilePath& expected_private_key_path)
    : expected_crx_path_(expected_crx_path),
      expected_private_key_path_(expected_private_key_path) {}

// If packing succeeded, we make sure that the package names match our
// expectations.
void PackExtensionTestClient::OnPackSuccess(const FilePath& crx_path,
                                            const FilePath& private_key_path) {
  // We got the notification and processed it; we don't expect any further tasks
  // to be posted to the current thread, so we should stop blocking and continue
  // on with the rest of the test.
  // This call to |Quit()| matches the call to |Run()| in the
  // |PackPunctuatedExtension| test.
  MessageLoop::current()->Quit();
  EXPECT_EQ(expected_crx_path_.value(), crx_path.value());
  EXPECT_EQ(expected_private_key_path_.value(), private_key_path.value());
  ASSERT_TRUE(file_util::PathExists(private_key_path));
}

// The tests are designed so that we never expect to see a packing error.
void PackExtensionTestClient::OnPackFailure(const std::string& error_message) {
  FAIL() << "Packing should not fail.";
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectorySuccess) {
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
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service_->Init();

  // On Chrome OS, we disallow extensions with plugins.  "good1" has plugins,
  // so we need to edit it out here.
  uint32 expected_num_extensions = 3u;
#if defined(OS_CHROMEOS)
  --expected_num_extensions;
#endif
  ASSERT_EQ(expected_num_extensions, loaded_.size());

  EXPECT_EQ(std::string(good0), loaded_[0]->id());
  EXPECT_EQ(std::string("My extension 1"),
            loaded_[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_[0]->description());
  EXPECT_EQ(Extension::INTERNAL, loaded_[0]->location());
  EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id(), false));
  EXPECT_EQ(expected_num_extensions, service_->extensions()->size());

  ValidatePrefKeyCount(3);
  ValidateIntegerPref(good0, "state", Extension::ENABLED);
  ValidateIntegerPref(good0, "location", Extension::INTERNAL);
  ValidateIntegerPref(good1, "state", Extension::ENABLED);
  ValidateIntegerPref(good1, "location", Extension::INTERNAL);
  ValidateIntegerPref(good2, "state", Extension::ENABLED);
  ValidateIntegerPref(good2, "location", Extension::INTERNAL);

  const Extension* extension = loaded_[0];
  const UserScriptList& scripts = extension->content_scripts();
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(3u, scripts[0].url_patterns().size());
  EXPECT_EQ("file://*",
            scripts[0].url_patterns()[0].GetAsString());
  EXPECT_EQ("http://*.google.com/*",
            scripts[0].url_patterns()[1].GetAsString());
  EXPECT_EQ("https://*.google.com/*",
            scripts[0].url_patterns()[2].GetAsString());
  EXPECT_EQ(2u, scripts[0].js_scripts().size());
  ExtensionResource resource00(extension->id(),
                               scripts[0].js_scripts()[0].extension_root(),
                               scripts[0].js_scripts()[0].relative_path());
  FilePath expected_path(extension->path().AppendASCII("script1.js"));
  ASSERT_TRUE(file_util::AbsolutePath(&expected_path));
  EXPECT_TRUE(resource00.ComparePathWithDefault(expected_path));
  ExtensionResource resource01(extension->id(),
                               scripts[0].js_scripts()[1].extension_root(),
                               scripts[0].js_scripts()[1].relative_path());
  expected_path = extension->path().AppendASCII("script2.js");
  ASSERT_TRUE(file_util::AbsolutePath(&expected_path));
  EXPECT_TRUE(resource01.ComparePathWithDefault(expected_path));
  EXPECT_TRUE(extension->plugins().empty());
  EXPECT_EQ(1u, scripts[1].url_patterns().size());
  EXPECT_EQ("http://*.news.com/*", scripts[1].url_patterns()[0].GetAsString());
  ExtensionResource resource10(extension->id(),
                               scripts[1].js_scripts()[0].extension_root(),
                               scripts[1].js_scripts()[0].relative_path());
  expected_path =
      extension->path().AppendASCII("js_files").AppendASCII("script3.js");
  ASSERT_TRUE(file_util::AbsolutePath(&expected_path));
  EXPECT_TRUE(resource10.ComparePathWithDefault(expected_path));
  const std::vector<URLPattern> permissions = extension->host_permissions();
  ASSERT_EQ(2u, permissions.size());
  EXPECT_EQ("http://*.google.com/*", permissions[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*", permissions[1].GetAsString());

#if !defined(OS_CHROMEOS)
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
#endif

  int index = expected_num_extensions - 1;
  EXPECT_EQ(std::string(good2), loaded_[index]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_[index]->name());
  EXPECT_EQ(std::string(""), loaded_[index]->description());
  EXPECT_EQ(0u, loaded_[index]->content_scripts().size());
  EXPECT_EQ(Extension::INTERNAL, loaded_[index]->location());
};

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectoryFail) {
  // Initialize the test dir with a bad Preferences/extensions.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("bad")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service_->Init();
  loop_.RunAllPending();

  ASSERT_EQ(4u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());

  EXPECT_TRUE(MatchPattern(GetErrors()[0],
      std::string("Could not load extension from '*'. ") +
      extension_manifest_errors::kManifestUnreadable)) << GetErrors()[0];

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
TEST_F(ExtensionServiceTest, CleanupOnStartup) {
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // Simulate that one of them got partially deleted by clearing its pref.
  DictionaryValue* dict =
      profile_->GetPrefs()->GetMutableDictionary("extensions.settings");
  ASSERT_TRUE(dict != NULL);
  dict->Remove("behllobkkfkfnphdnhnkndlbkcpglgmj", NULL);

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
TEST_F(ExtensionServiceTest, InstallExtension) {
  InitializeEmptyExtensionService();

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
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Extension::INTERNAL);

  // An extension with page actions.
  path = extensions_path.AppendASCII("page_action.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(page_action, "state", Extension::ENABLED);
  ValidateIntegerPref(page_action, "location", Extension::INTERNAL);

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

  // Extensions cannot have folders or files that have underscores except in
  // certain whitelisted cases (eg _locales). This is an example of a broader
  // class of validation that we do to the directory structure of the extension.
  // We did not used to handle this correctly for installation.
  path = extensions_path.AppendASCII("bad_underscore.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

// Test the handling of killed extensions.
TEST_F(ExtensionServiceTest, KilledExtensions) {
  InitializeEmptyExtensionService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  set_extensions_enabled(true);

  scoped_ptr<Version> version;
  version.reset(Version::GetVersionFromString("1.0.0.0"));
  // Install an external extension.
  service_->OnExternalExtensionFileFound(good_crx, version.get(),
                                         path, Extension::EXTERNAL_PREF);
  loop_.RunAllPending();
  ASSERT_TRUE(NULL != service_->GetExtensionById(good_crx, false));

  // Uninstall it and check that its killbit gets set.
  service_->UninstallExtension(good_crx, false);
  loop_.RunAllPending();
  ValidateIntegerPref(good_crx, "location", Extension::KILLBIT);

  // Try to re-install it externally. This should fail because of the killbit.
  service_->OnExternalExtensionFileFound(good_crx, version.get(),
                                         path, Extension::EXTERNAL_PREF);
  loop_.RunAllPending();
  ASSERT_TRUE(NULL == service_->GetExtensionById(good_crx, false));
  ValidateIntegerPref(good_crx, "location", Extension::KILLBIT);

  version.reset(Version::GetVersionFromString("1.0.0.1"));
  // Repeat the same thing with a newer version of the extension.
  path = extensions_path.AppendASCII("good2.crx");
  service_->OnExternalExtensionFileFound(good_crx, version.get(),
                                         path, Extension::EXTERNAL_PREF);
  loop_.RunAllPending();
  ASSERT_TRUE(NULL == service_->GetExtensionById(good_crx, false));
  ValidateIntegerPref(good_crx, "location", Extension::KILLBIT);

  // Try adding the same extension from an external update URL.
  service_->AddPendingExtensionFromExternalUpdateUrl(
      good_crx,
      GURL("http:://fake.update/url"),
      Extension::EXTERNAL_PREF_DOWNLOAD);
  const PendingExtensionMap& pending_extensions =
      service_->pending_extensions();
  ASSERT_TRUE(pending_extensions.find(good_crx) == pending_extensions.end());
}

// Install a user script (they get converted automatically to an extension)
TEST_F(ExtensionServiceTest, InstallUserScript) {
  // The details of script conversion are tested elsewhere, this just tests
  // integration with ExtensionService.
  InitializeEmptyExtensionService();

  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions")
             .AppendASCII("user_script_basic.user.js");

  ASSERT_TRUE(file_util::PathExists(path));
  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(service_, NULL));  // silent install
  installer->InstallUserScript(
      path,
      GURL("http://www.aaronboodman.com/scripts/user_script_basic.user.js"));

  loop_.RunAllPending();
  std::vector<std::string> errors = GetErrors();
  EXPECT_TRUE(installed_) << "Nothing was installed.";
  ASSERT_EQ(1u, loaded_.size()) << "Nothing was loaded.";
  EXPECT_EQ(0u, errors.size()) << "There were errors: "
                               << JoinString(errors, ',');
  EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id(), false)) <<
              path.value();

  installed_ = NULL;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();
}

// This tests that the granted permissions preferences are correctly set when
// installing an extension.
TEST_F(ExtensionServiceTest, GrantedPermissions) {
  InitializeEmptyExtensionService();
  FilePath path;
  FilePath pem_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions")
      .AppendASCII("permissions");

  pem_path = path.AppendASCII("unknown.pem");
  path = path.AppendASCII("unknown");

  ASSERT_TRUE(file_util::PathExists(pem_path));
  ASSERT_TRUE(file_util::PathExists(path));

  ExtensionPrefs* prefs = service_->extension_prefs();

  std::set<std::string> expected_api_perms;
  std::set<std::string> known_api_perms;
  bool full_access;
  ExtensionExtent expected_host_perms;
  ExtensionExtent known_host_perms;

  // Make sure there aren't any granted permissions before the
  // extension is installed.
  EXPECT_FALSE(prefs->GetGrantedPermissions(
      permissions_crx, &full_access, &known_api_perms, &known_host_perms));
  EXPECT_TRUE(known_api_perms.empty());
  EXPECT_TRUE(known_host_perms.is_empty());

  PackAndInstallExtension(path, pem_path, true);

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, service_->extensions()->size());
  std::string extension_id = service_->extensions()->at(0)->id();
  EXPECT_EQ(permissions_crx, extension_id);


  // Verify that the valid API permissions have been recognized.
  expected_api_perms.insert("tabs");

  AddPattern(&expected_host_perms, "http://*.google.com/*");
  AddPattern(&expected_host_perms, "https://*.google.com/*");
  AddPattern(&expected_host_perms, "http://*.google.com.hk/*");
  AddPattern(&expected_host_perms, "http://www.example.com/*");

  EXPECT_TRUE(prefs->GetGrantedPermissions(extension_id,
                                           &full_access,
                                           &known_api_perms,
                                           &known_host_perms));

  EXPECT_EQ(expected_api_perms, known_api_perms);
  EXPECT_FALSE(full_access);
  AssertEqualExtents(&expected_host_perms, &known_host_perms);
}

#if !defined(OS_CHROMEOS)
// Tests that the granted permissions full_access bit gets set correctly when
// an extension contains an NPAPI plugin. Don't run this test on Chrome OS
// since they don't support plugins.
TEST_F(ExtensionServiceTest, GrantedFullAccessPermissions) {
  InitializeEmptyExtensionService();

  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII(good1)
      .AppendASCII("2");

  ASSERT_TRUE(file_util::PathExists(path));

  PackAndInstallExtension(path, true);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, service_->extensions()->size());
  const Extension* extension = service_->extensions()->at(0);
  std::string extension_id = extension->id();
  ExtensionPrefs* prefs = service_->extension_prefs();

  bool full_access;
  std::set<std::string> api_permissions;
  ExtensionExtent host_permissions;
  EXPECT_TRUE(prefs->GetGrantedPermissions(
      extension_id, &full_access, &api_permissions, &host_permissions));

  EXPECT_TRUE(full_access);
  EXPECT_TRUE(api_permissions.empty());
  EXPECT_TRUE(host_permissions.is_empty());
}
#endif

// Tests that the extension is disabled when permissions are missing from
// the extension's granted permissions preferences. (This simulates updating
// the browser to a version which recognizes more permissions).
TEST_F(ExtensionServiceTest, GrantedAPIAndHostPermissions) {
  InitializeEmptyExtensionService();

  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions")
      .AppendASCII("permissions")
      .AppendASCII("unknown");

  ASSERT_TRUE(file_util::PathExists(path));

  PackAndInstallExtension(path, true);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, service_->extensions()->size());
  const Extension* extension = service_->extensions()->at(0);
  std::string extension_id = extension->id();

  ExtensionPrefs* prefs = service_->extension_prefs();

  std::set<std::string> expected_api_permissions;
  ExtensionExtent expected_host_permissions;

  expected_api_permissions.insert("tabs");
  AddPattern(&expected_host_permissions, "http://*.google.com/*");
  AddPattern(&expected_host_permissions, "https://*.google.com/*");
  AddPattern(&expected_host_permissions, "http://*.google.com.hk/*");
  AddPattern(&expected_host_permissions, "http://www.example.com/*");

  std::set<std::string> api_permissions;
  std::set<std::string> host_permissions;

  // Test that the extension is disabled when an API permission is missing from
  // the extension's granted api permissions preference. (This simulates
  // updating the browser to a version which recognizes a new API permission).
  SetPrefStringSet(extension_id, "granted_permissions.api", api_permissions);

  service_->ReloadExtensions();

  EXPECT_EQ(1u, service_->disabled_extensions()->size());
  extension = service_->disabled_extensions()->at(0);

  ASSERT_TRUE(prefs->GetExtensionState(extension_id) == Extension::DISABLED);
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service_->GrantPermissionsAndEnableExtension(extension);

  ASSERT_TRUE(prefs->GetExtensionState(extension_id) == Extension::ENABLED);
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  std::set<std::string> current_api_permissions;
  ExtensionExtent current_host_permissions;
  bool current_full_access;

  ASSERT_TRUE(prefs->GetGrantedPermissions(extension_id,
                                           &current_full_access,
                                           &current_api_permissions,
                                           &current_host_permissions));

  ASSERT_FALSE(current_full_access);
  ASSERT_EQ(expected_api_permissions, current_api_permissions);
  AssertEqualExtents(&expected_host_permissions, &current_host_permissions);

  // Tests that the extension is disabled when a host permission is missing from
  // the extension's granted host permissions preference. (This simulates
  // updating the browser to a version which recognizes additional host
  // permissions).
  api_permissions.clear();
  host_permissions.clear();
  current_api_permissions.clear();
  current_host_permissions.ClearPaths();

  api_permissions.insert("tabs");
  host_permissions.insert("http://*.google.com/*");
  host_permissions.insert("https://*.google.com/*");
  host_permissions.insert("http://*.google.com.hk/*");

  SetPrefStringSet(extension_id, "granted_permissions.api", api_permissions);
  SetPrefStringSet(extension_id, "granted_permissions.host", host_permissions);

  service_->ReloadExtensions();

  EXPECT_EQ(1u, service_->disabled_extensions()->size());
  extension = service_->disabled_extensions()->at(0);

  ASSERT_TRUE(prefs->GetExtensionState(extension_id) == Extension::DISABLED);
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service_->GrantPermissionsAndEnableExtension(extension);

  ASSERT_TRUE(prefs->GetExtensionState(extension_id) == Extension::ENABLED);
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  ASSERT_TRUE(prefs->GetGrantedPermissions(extension_id,
                                           &current_full_access,
                                           &current_api_permissions,
                                           &current_host_permissions));

  ASSERT_FALSE(current_full_access);
  ASSERT_EQ(expected_api_permissions, current_api_permissions);
  AssertEqualExtents(&expected_host_permissions, &current_host_permissions);

  // Tests that the granted permissions preferences are initialized when
  // migrating from the old pref schema.
  current_api_permissions.clear();
  current_host_permissions.ClearPaths();

  ClearPref(extension_id, "granted_permissions");

  service_->ReloadExtensions();

  EXPECT_EQ(1u, service_->extensions()->size());
  extension = service_->extensions()->at(0);

  ASSERT_TRUE(prefs->GetExtensionState(extension_id) == Extension::ENABLED);
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  ASSERT_TRUE(prefs->GetGrantedPermissions(extension_id,
                                           &current_full_access,
                                           &current_api_permissions,
                                           &current_host_permissions));

  ASSERT_FALSE(current_full_access);
  ASSERT_EQ(expected_api_permissions, current_api_permissions);
  AssertEqualExtents(&expected_host_permissions, &current_host_permissions);
}

// Test Packaging and installing an extension.
TEST_F(ExtensionServiceTest, PackExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath output_directory = temp_dir.path();

  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, FilePath(),
      privkey_path));

  ASSERT_TRUE(file_util::PathExists(privkey_path));
  InstallExtension(crx_path, true);

  // Try packing with invalid paths.
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(creator->Run(FilePath(), FilePath(), FilePath(), FilePath()));

  // Try packing an empty directory. Should fail because an empty directory is
  // not a valid extension.
  ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(creator->Run(temp_dir2.path(), crx_path, privkey_path,
                            FilePath()));

  // Try packing with an invalid manifest.
  std::string invalid_manifest_content = "I am not a manifest.";
  ASSERT_TRUE(file_util::WriteFile(
      temp_dir2.path().Append(Extension::kManifestFilename),
      invalid_manifest_content.c_str(), invalid_manifest_content.size()));
  creator.reset(new ExtensionCreator());
  ASSERT_FALSE(creator->Run(temp_dir2.path(), crx_path, privkey_path,
                            FilePath()));
}

// Test Packaging and installing an extension whose name contains punctuation.
TEST_F(ExtensionServiceTest, PackPunctuatedExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII(good0)
      .AppendASCII("1.0.0.0");

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Extension names containing punctuation, and the expected names for the
  // packed extensions.
  const FilePath punctuated_names[] = {
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("this.extensions.name.has.periods"))),
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod"))),
    NormalizeSeparators(FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("thisextensionhasaslashinitsname/")))),
  };
  const FilePath expected_crx_names[] = {
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("this.extensions.name.has.periods.crx"))),
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.crx"))),
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("thisextensionhasaslashinitsname.crx"))),
  };
  const FilePath expected_private_key_names[] = {
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("this.extensions.name.has.periods.pem"))),
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.pem"))),
    FilePath(FilePath::StringType(
        FILE_PATH_LITERAL("thisextensionhasaslashinitsname.pem"))),
  };

  for (size_t i = 0; i < arraysize(punctuated_names); ++i) {
    SCOPED_TRACE(punctuated_names[i].value().c_str());
    FilePath output_dir = temp_dir.path().Append(punctuated_names[i]);

    // Copy the extension into the output directory, as PackExtensionJob doesn't
    // let us choose where to output the packed extension.
    ASSERT_TRUE(file_util::CopyDirectory(input_directory, output_dir, true));

    FilePath expected_crx_path = temp_dir.path().Append(expected_crx_names[i]);
    FilePath expected_private_key_path =
        temp_dir.path().Append(expected_private_key_names[i]);
    PackExtensionTestClient pack_client(expected_crx_path,
                                        expected_private_key_path);
    scoped_refptr<PackExtensionJob> packer(new PackExtensionJob(&pack_client,
                                                                output_dir,
                                                                FilePath()));
    packer->Start();

    // The packer will post a notification task to the current thread's message
    // loop when it is finished.  We manually run the loop here so that we
    // block and catch the notification; otherwise, the process would exit.
    // This call to |Run()| is matched by a call to |Quit()| in the
    // |PackExtensionTestClient|'s notification handling code.
    MessageLoop::current()->Run();

    if (HasFatalFailure())
      return;

    InstallExtension(expected_crx_path, true);
  }
}

// Test Packaging and installing an extension using an openssl generated key.
// The openssl is generated with the following:
// > openssl genrsa -out privkey.pem 1024
// > openssl pkcs8 -topk8 -nocrypt -in privkey.pem -out privkey_asn1.pem
// The privkey.pem is a PrivateKey, and the pcks8 -topk8 creates a
// PrivateKeyInfo ASN.1 structure, we our RSAPrivateKey expects.
TEST_F(ExtensionServiceTest, PackExtensionOpenSSLKey) {
  InitializeEmptyExtensionService();
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

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath output_directory = temp_dir.path();

  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, privkey_path,
      FilePath()));

  InstallExtension(crx_path, true);
}

TEST_F(ExtensionServiceTest, InstallTheme) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A theme.
  FilePath path = extensions_path.AppendASCII("theme.crx");
  InstallExtension(path, true);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme_crx, "location", Extension::INTERNAL);

  // A theme when extensions are disabled. Themes can be installed, even when
  // extensions are disabled.
  set_extensions_enabled(false);
  path = extensions_path.AppendASCII("theme2.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme2_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme2_crx, "location", Extension::INTERNAL);

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

TEST_F(ExtensionServiceTest, LoadLocalizedTheme) {
  // Load.
  InitializeEmptyExtensionService();
  FilePath extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
  extension_path = extension_path
      .AppendASCII("extensions")
      .AppendASCII("theme_i18n");

  service_->LoadExtension(extension_path);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ("name", service_->extensions()->at(0)->name());
  EXPECT_EQ("description", service_->extensions()->at(0)->description());
}

TEST_F(ExtensionServiceTest, InstallLocalizedTheme) {
  InitializeEmptyExtensionService();
  FilePath theme_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &theme_path));
  theme_path = theme_path
      .AppendASCII("extensions")
      .AppendASCII("theme_i18n");

  PackAndInstallExtension(theme_path, true);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ("name", service_->extensions()->at(0)->name());
  EXPECT_EQ("description", service_->extensions()->at(0)->description());
}

TEST_F(ExtensionServiceTest, InstallApps) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // An empty app.
  PackAndInstallExtension(extensions_path.AppendASCII("app1"), true);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, service_->extensions()->size());
  std::string id = service_->extensions()->at(0)->id();
  ValidateIntegerPref(id, "state", Extension::ENABLED);
  ValidateIntegerPref(id, "location", Extension::INTERNAL);

  // Another app with non-overlapping extent. Should succeed.
  PackAndInstallExtension(extensions_path.AppendASCII("app2"), true);
  ValidatePrefKeyCount(++pref_count);

  // A third app whose extent overlaps the first. Should fail.
  PackAndInstallExtension(extensions_path.AppendASCII("app3"), false);
  ValidatePrefKeyCount(pref_count);
}

TEST_F(ExtensionServiceTest, UpdateApps) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path =
      extensions_path.AppendASCII("extensions").AppendASCII("app_update");

  // First install v1 of a hosted app.
  InstallExtension(extensions_path.AppendASCII("v1.crx"), true);
  ASSERT_EQ(1u, service_->extensions()->size());
  std::string id = service_->extensions()->at(0)->id();
  ASSERT_EQ(std::string("1"),
            service_->extensions()->at(0)->version()->GetString());

  // Now try updating to v2.
  UpdateExtension(id,
                  extensions_path.AppendASCII("v2.crx"),
                  ENABLED);
  ASSERT_EQ(std::string("2"),
            service_->extensions()->at(0)->version()->GetString());
}

TEST_F(ExtensionServiceTest, InstallAppsWithUnlimtedStorage) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service_->extensions()->empty());
  EXPECT_TRUE(service_->unlimited_storage_map_.empty());

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  int pref_count = 0;
  ChromeAppCacheService* appcache_service = profile_->GetAppCacheService();

  // Install app1 with unlimited storage.
  PackAndInstallExtension(extensions_path.AppendASCII("app1"), true);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, service_->extensions()->size());
  const Extension* extension = service_->extensions()->at(0);
  const std::string id1 = extension->id();
  EXPECT_TRUE(extension->HasApiPermission(
                  Extension::kUnlimitedStoragePermission));
  EXPECT_TRUE(extension->web_extent().ContainsURL(
                  extension->GetFullLaunchURL()));
  const GURL origin1(extension->GetFullLaunchURL().GetOrigin());
  EXPECT_EQ(kint64max,
            appcache_service->storage()->GetOriginQuotaInMemory(origin1));
  EXPECT_FALSE(service_->unlimited_storage_map_.empty());

  // Install app2 from the same origin with unlimited storage.
  PackAndInstallExtension(extensions_path.AppendASCII("app2"), true);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, service_->extensions()->size());
  extension = service_->extensions()->at(1);
  const std::string id2 = extension->id();
  EXPECT_TRUE(extension->HasApiPermission(
                  Extension::kUnlimitedStoragePermission));
  EXPECT_TRUE(extension->web_extent().ContainsURL(
                  extension->GetFullLaunchURL()));
  const GURL origin2(extension->GetFullLaunchURL().GetOrigin());
  EXPECT_EQ(origin1, origin2);
  EXPECT_EQ(kint64max,
            appcache_service->storage()->GetOriginQuotaInMemory(origin2));
  EXPECT_FALSE(service_->unlimited_storage_map_.empty());

  // Uninstall one of them, unlimited storage should still be granted
  // to the origin.
  service_->UninstallExtension(id1, false);
  loop_.RunAllPending();
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(kint64max,
            appcache_service->storage()->GetOriginQuotaInMemory(origin1));
  EXPECT_FALSE(service_->unlimited_storage_map_.empty());

  // Uninstall the other, unlimited storage should be revoked.
  service_->UninstallExtension(id2, false);
  loop_.RunAllPending();
  EXPECT_EQ(0u, service_->extensions()->size());
  EXPECT_EQ(-1L,
            appcache_service->storage()->GetOriginQuotaInMemory(origin2));
  EXPECT_TRUE(service_->unlimited_storage_map_.empty());
}

TEST_F(ExtensionServiceTest, InstallAppsAndCheckStorageProtection) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service_->extensions()->empty());
  EXPECT_TRUE(service_->protected_storage_map_.empty());

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  int pref_count = 0;

  PackAndInstallExtension(extensions_path.AppendASCII("app1"), true);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, service_->extensions()->size());
  const Extension* extension = service_->extensions()->at(0);
  EXPECT_TRUE(extension->is_app());
  const std::string id1 = extension->id();
  EXPECT_FALSE(service_->protected_storage_map_.empty());
  const GURL origin1(extension->GetFullLaunchURL().GetOrigin());
  ASSERT_EQ(1, service_->protected_storage_map_[origin1]);

  // App 4 has a different origin (maps.google.com).
  PackAndInstallExtension(extensions_path.AppendASCII("app4"), true);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, service_->extensions()->size());
  extension = service_->extensions()->at(1);
  const std::string id2 = extension->id();
  EXPECT_FALSE(service_->protected_storage_map_.empty());
  const GURL origin2(extension->GetFullLaunchURL().GetOrigin());
  ASSERT_NE(origin1, origin2);
  ASSERT_EQ(1, service_->protected_storage_map_[origin2]);

  service_->UninstallExtension(id1, false);
  loop_.RunAllPending();
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_FALSE(service_->protected_storage_map_.empty());

  service_->UninstallExtension(id2, false);
  loop_.RunAllPending();

  EXPECT_TRUE(service_->extensions()->empty());
  EXPECT_TRUE(service_->protected_storage_map_.empty());
}

// Test that when an extension version is reinstalled, nothing happens.
TEST_F(ExtensionServiceTest, Reinstall) {
  InitializeEmptyExtensionService();
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
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Extension::INTERNAL);

  installed_ = NULL;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();

  // Reinstall the same version, it should overwrite the previous one.
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Extension::INTERNAL);
}

// Test upgrading a signed extension.
TEST_F(ExtensionServiceTest, UpgradeSignedGood) {
  InitializeEmptyExtensionService();
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
TEST_F(ExtensionServiceTest, UpgradeSignedBad) {
  InitializeEmptyExtensionService();
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
TEST_F(ExtensionServiceTest, UpdateExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  path = extensions_path.AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
}

// Test updating a not-already-installed extension - this should fail
TEST_F(ExtensionServiceTest, UpdateNotInstalledExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(good_crx, path, UPDATED);
  loop_.RunAllPending();

  ASSERT_EQ(0u, service_->extensions()->size());
  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
}

// Makes sure you can't downgrade an extension via UpdateExtension
TEST_F(ExtensionServiceTest, UpdateWillNotDowngrade) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good2.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.1", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Change path from good2.crx -> good.crx
  path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(good_crx, path, FAILED);
  ASSERT_EQ("1.0.0.1", service_->extensions()->at(0)->VersionString());
}

// Make sure calling update with an identical version does nothing
TEST_F(ExtensionServiceTest, UpdateToSameVersionIsNoop) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);
  ASSERT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);
}

// Tests that updating an extension does not clobber old state.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesState) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Disable it and allow it to run in incognito. These settings should carry
  // over to the updated version.
  service_->DisableExtension(good->id());
  service_->SetIsIncognitoEnabled(good, true);

  path = extensions_path.AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, INSTALLED);
  ASSERT_EQ(1u, service_->disabled_extensions()->size());
  const Extension* good2 = service_->disabled_extensions()->at(0);
  ASSERT_EQ("1.0.0.1", good2->version()->GetString());
  EXPECT_TRUE(service_->IsIncognitoEnabled(good2));
}

// Tests that updating preserves extension location.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesLocation) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);

  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Simulate non-internal location.
  const_cast<Extension*>(good)->location_ = Extension::EXTERNAL_PREF;

  path = extensions_path.AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  const Extension* good2 = service_->extensions()->at(0);
  ASSERT_EQ("1.0.0.1", good2->version()->GetString());
  EXPECT_EQ(good2->location(), Extension::EXTERNAL_PREF);
}

// Makes sure that LOAD extension types can downgrade.
TEST_F(ExtensionServiceTest, LoadExtensionsCanDowngrade) {
  InitializeEmptyExtensionService();

  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // We'll write the extension manifest dynamically to a temporary path
  // to make it easier to change the version number.
  FilePath extension_path = temp.path();
  FilePath manifest_path = extension_path.Append(Extension::kManifestFilename);
  ASSERT_FALSE(file_util::PathExists(manifest_path));

  // Start with version 2.0.
  DictionaryValue manifest;
  manifest.SetString("version", "2.0");
  manifest.SetString("name", "LOAD Downgrade Test");

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(manifest));

  service_->LoadExtension(extension_path);
  loop_.RunAllPending();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Extension::LOAD, loaded_[0]->location());
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ("2.0", loaded_[0]->VersionString());

  // Now set the version number to 1.0, reload the extensions and verify that
  // the downgrade was accepted.
  manifest.SetString("version", "1.0");
  ASSERT_TRUE(serializer.Serialize(manifest));

  service_->LoadExtension(extension_path);
  loop_.RunAllPending();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Extension::LOAD, loaded_[0]->location());
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ("1.0", loaded_[0]->VersionString());
}

namespace {

bool IsExtension(const Extension& extension) {
  return extension.GetType() == Extension::TYPE_EXTENSION;
}

}  // namespace

// Test adding a pending extension.
TEST_F(ExtensionServiceTest, AddPendingExtensionFromSync) {
  InitializeEmptyExtensionService();

  const std::string kFakeId("fake-id");
  const GURL kFakeUpdateURL("http:://fake.update/url");
  const bool kFakeInstallSilently(true);
  const Extension::State kFakeInitialState(Extension::ENABLED);
  const bool kFakeInitialIncognitoEnabled(false);

  service_->AddPendingExtensionFromSync(
      kFakeId, kFakeUpdateURL, &IsExtension,
      kFakeInstallSilently, kFakeInitialState, kFakeInitialIncognitoEnabled);
  PendingExtensionMap::const_iterator it =
      service_->pending_extensions().find(kFakeId);
  ASSERT_TRUE(it != service_->pending_extensions().end());
  EXPECT_EQ(kFakeUpdateURL, it->second.update_url);
  EXPECT_EQ(&IsExtension, it->second.should_install_extension);
  EXPECT_EQ(kFakeInstallSilently, it->second.install_silently);
}

namespace {
const char kGoodId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodUpdateURL[] = "http://good.update/url";
const bool kGoodIsFromSync = true;
const bool kGoodInstallSilently = true;
const Extension::State kGoodInitialState = Extension::DISABLED;
const bool kGoodInitialIncognitoEnabled = true;
}  // namespace

// Test updating a pending extension.
TEST_F(ExtensionServiceTest, UpdatePendingExtension) {
  InitializeEmptyExtensionService();
  service_->AddPendingExtensionFromSync(
      kGoodId, GURL(kGoodUpdateURL), &IsExtension,
      kGoodInstallSilently, kGoodInitialState,
      kGoodInitialIncognitoEnabled);
  EXPECT_TRUE(ContainsKey(service_->pending_extensions(), kGoodId));

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, INSTALLED);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), kGoodId));

  const Extension* extension = service_->GetExtensionById(kGoodId, true);
  ASSERT_TRUE(extension);

  bool enabled = service_->GetExtensionById(kGoodId, false);
  EXPECT_EQ(kGoodInitialState == Extension::ENABLED, enabled);
  EXPECT_EQ(kGoodInitialState,
            service_->extension_prefs()->GetExtensionState(extension->id()));
  EXPECT_EQ(kGoodInitialIncognitoEnabled,
            service_->IsIncognitoEnabled(extension));
}

namespace {

bool IsTheme(const Extension& extension) {
  return extension.is_theme();
}

}  // namespace

// Test updating a pending theme.
TEST_F(ExtensionServiceTest, UpdatePendingTheme) {
  InitializeEmptyExtensionService();
  service_->AddPendingExtensionFromSync(
      theme_crx, GURL(), &IsTheme,
      false, Extension::ENABLED, false);
  EXPECT_TRUE(ContainsKey(service_->pending_extensions(), theme_crx));

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), theme_crx));

  const Extension* extension = service_->GetExtensionById(theme_crx, true);
  ASSERT_TRUE(extension);

  EXPECT_EQ(Extension::ENABLED,
            service_->extension_prefs()->GetExtensionState(extension->id()));
  EXPECT_FALSE(service_->IsIncognitoEnabled(extension));
}

// Test updating a pending CRX as if the source is an external extension
// with an update URL.  In this case we don't know if the CRX is a theme
// or not.
TEST_F(ExtensionServiceTest, UpdatePendingExternalCrx) {
  InitializeEmptyExtensionService();
  service_->AddPendingExtensionFromExternalUpdateUrl(
      theme_crx, GURL(), Extension::EXTERNAL_PREF_DOWNLOAD);

  EXPECT_TRUE(ContainsKey(service_->pending_extensions(), theme_crx));

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), theme_crx));

  const Extension* extension = service_->GetExtensionById(theme_crx, true);
  ASSERT_TRUE(extension);

  EXPECT_EQ(Extension::ENABLED,
            service_->extension_prefs()->GetExtensionState(extension->id()));
  EXPECT_FALSE(service_->IsIncognitoEnabled(extension));
}

// Test updating a pending CRX as if the source is an external extension
// with an update URL.  The external update should overwrite a sync update,
// but a sync update should not overwrite a non-sync update.
TEST_F(ExtensionServiceTest, UpdatePendingExternalCrxWinsOverSync) {
  InitializeEmptyExtensionService();

  // Add a crx to be installed from the update mechanism.
  service_->AddPendingExtensionFromSync(
      kGoodId, GURL(kGoodUpdateURL), &IsExtension,
      kGoodInstallSilently, kGoodInitialState,
      kGoodInitialIncognitoEnabled);

  // Check that there is a pending crx, with is_from_sync set to true.
  PendingExtensionMap::const_iterator it;
  it = service_->pending_extensions().find(kGoodId);
  ASSERT_TRUE(it != service_->pending_extensions().end());
  EXPECT_TRUE(it->second.is_from_sync);

  // Add a crx to be updated, with the same ID, from a non-sync source.
  service_->AddPendingExtensionFromExternalUpdateUrl(
      kGoodId, GURL(kGoodUpdateURL), Extension::EXTERNAL_PREF_DOWNLOAD);

  // Check that there is a pending crx, with is_from_sync set to false.
  it = service_->pending_extensions().find(kGoodId);
  ASSERT_TRUE(it != service_->pending_extensions().end());
  EXPECT_FALSE(it->second.is_from_sync);

  // Add a crx to be installed from the update mechanism.
  service_->AddPendingExtensionFromSync(
      kGoodId, GURL(kGoodUpdateURL), &IsExtension,
      kGoodInstallSilently, kGoodInitialState,
      kGoodInitialIncognitoEnabled);

  // Check that the external, non-sync update was not overridden.
  it = service_->pending_extensions().find(kGoodId);
  ASSERT_TRUE(it != service_->pending_extensions().end());
  EXPECT_FALSE(it->second.is_from_sync);
}

// Updating a theme should fail if the updater is explicitly told that
// the CRX is not a theme.
TEST_F(ExtensionServiceTest, UpdatePendingCrxThemeMismatch) {
  InitializeEmptyExtensionService();
  service_->AddPendingExtensionFromSync(
      theme_crx, GURL(), &IsExtension,
      true, Extension::ENABLED, false);

  EXPECT_TRUE(ContainsKey(service_->pending_extensions(), theme_crx));

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, FAILED_SILENTLY);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), theme_crx));

  const Extension* extension = service_->GetExtensionById(theme_crx, true);
  ASSERT_FALSE(extension);
}

// TODO(akalin): Test updating a pending extension non-silently once
// we can mock out ExtensionInstallUI and inject our version into
// UpdateExtension().

// Test updating a pending extension which fails the should-install test.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionFailedShouldInstallTest) {
  InitializeEmptyExtensionService();
  // Add pending extension with a flipped is_theme.
  service_->AddPendingExtensionFromSync(
      kGoodId, GURL(kGoodUpdateURL), &IsTheme,
      kGoodInstallSilently, kGoodInitialState,
      kGoodInitialIncognitoEnabled);
  EXPECT_TRUE(ContainsKey(service_->pending_extensions(), kGoodId));

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  // TODO(akalin): Figure out how to check that the extensions
  // directory is cleaned up properly in OnExtensionInstalled().

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), kGoodId));
}

// TODO(akalin): Figure out how to test that installs of pending
// unsyncable extensions are blocked.

// Test updating a pending extension for one that is not pending.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionNotPending) {
  InitializeEmptyExtensionService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), kGoodId));
}

// Test updating a pending extension for one that is already
// installed.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionAlreadyInstalled) {
  InitializeEmptyExtensionService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);
  ASSERT_EQ(1u, service_->extensions()->size());
  const Extension* good = service_->extensions()->at(0);

  EXPECT_FALSE(good->is_theme());

  // Use AddPendingExtensionInternal() as AddPendingExtension() would
  // balk.
  service_->AddPendingExtensionInternal(
      good->id(), good->update_url(), &IsExtension,
      kGoodIsFromSync, kGoodInstallSilently, kGoodInitialState,
      kGoodInitialIncognitoEnabled, Extension::INTERNAL);
  UpdateExtension(good->id(), path, INSTALLED);

  EXPECT_FALSE(ContainsKey(service_->pending_extensions(), kGoodId));
}

// Test pref settings for blacklist and unblacklist extensions.
TEST_F(ExtensionServiceTest, SetUnsetBlacklistInPrefs) {
  InitializeEmptyExtensionService();
  std::vector<std::string> blacklist;
  blacklist.push_back(good0);
  blacklist.push_back("invalid_id");  // an invalid id
  blacklist.push_back(good1);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // blacklist is set for good0,1,2
  ValidateBooleanPref(good0, "blacklist", true);
  ValidateBooleanPref(good1, "blacklist", true);
  // invalid_id should not be inserted to pref.
  EXPECT_FALSE(IsPrefExist("invalid_id", "blacklist"));

  // remove good1, add good2
  blacklist.pop_back();
  blacklist.push_back(good2);

  service_->UpdateExtensionBlacklist(blacklist);
  // only good0 and good1 should be set
  ValidateBooleanPref(good0, "blacklist", true);
  EXPECT_FALSE(IsPrefExist(good1, "blacklist"));
  ValidateBooleanPref(good2, "blacklist", true);
  EXPECT_FALSE(IsPrefExist("invalid_id", "blacklist"));
}

// Unload installed extension from blacklist.
TEST_F(ExtensionServiceTest, UnloadBlacklistedExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");

  InstallExtension(path, true);
  const Extension* good = service_->extensions()->at(0);
  EXPECT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);

  std::vector<std::string> blacklist;
  blacklist.push_back(good_crx);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // Now, the good_crx is blacklisted.
  ValidateBooleanPref(good_crx, "blacklist", true);
  EXPECT_EQ(0u, service_->extensions()->size());

  // Remove good_crx from blacklist
  blacklist.pop_back();
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();
  // blacklist value should not be set for good_crx
  EXPECT_FALSE(IsPrefExist(good_crx, "blacklist"));
}

// Unload installed extension from blacklist.
TEST_F(ExtensionServiceTest, BlacklistedExtensionWillNotInstall) {
  InitializeEmptyExtensionService();
  std::vector<std::string> blacklist;
  blacklist.push_back(good_crx);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  // Now, the good_crx is blacklisted.
  ValidateBooleanPref(good_crx, "blacklist", true);

  // We can not install good_crx.
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();
  EXPECT_EQ(0u, service_->extensions()->size());
  ValidateBooleanPref(good_crx, "blacklist", true);
}

// Test loading extensions from the profile directory, except
// blacklisted ones.
TEST_F(ExtensionServiceTest, WillNotLoadBlacklistedExtensionsFromDirectory) {
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
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // Blacklist good1.
  std::vector<std::string> blacklist;
  blacklist.push_back(good1);
  service_->UpdateExtensionBlacklist(blacklist);
  // Make sure pref is updated
  loop_.RunAllPending();

  ValidateBooleanPref(good1, "blacklist", true);

  // Load extensions.
  service_->Init();
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(2u, loaded_.size());

  EXPECT_NE(std::string(good1), loaded_[0]->id());
  EXPECT_NE(std::string(good1), loaded_[1]->id());
}

#if defined(OS_CHROMEOS)
// Test loading extensions from the profile directory, except
// ones with a plugin.
TEST_F(ExtensionServiceTest, WillNotLoadPluginExtensionsFromDirectory) {
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
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // good1 contains a plugin.
  // Load extensions.
  service_->Init();
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(2u, loaded_.size());

  EXPECT_NE(std::string(good1), loaded_[0]->id());
  EXPECT_NE(std::string(good1), loaded_[1]->id());
}
#endif

// Will not install extension blacklisted by policy.
TEST_F(ExtensionServiceTest, BlacklistedByPolicyWillNotInstall) {
  InitializeEmptyExtensionService();

  ListValue* whitelist =
      profile_->GetPrefs()->GetMutableList(prefs::kExtensionInstallAllowList);
  ListValue* blacklist =
      profile_->GetPrefs()->GetMutableList(prefs::kExtensionInstallDenyList);
  ASSERT_TRUE(whitelist != NULL && blacklist != NULL);

  // Blacklist everything.
  blacklist->Append(Value::CreateStringValue("*"));

  // Blacklist prevents us from installing good_crx.
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();
  EXPECT_EQ(0u, service_->extensions()->size());

  // Now whitelist this particular extension.
  whitelist->Append(Value::CreateStringValue(good_crx));

  // Ensure we can now install good_crx.
  service_->InstallExtension(path);
  loop_.RunAllPending();
  EXPECT_EQ(1u, service_->extensions()->size());
}

// Extension blacklisted by policy get unloaded after installing.
TEST_F(ExtensionServiceTest, BlacklistedByPolicyRemovedIfRunning) {
  InitializeEmptyExtensionService();

  // Install good_crx.
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();
  EXPECT_EQ(1u, service_->extensions()->size());

  { // Scope for pref update notification.
    PrefService* prefs = profile_->GetPrefs();
    ScopedPrefUpdate pref_update(prefs, prefs::kExtensionInstallDenyList);
    ListValue* blacklist =
        prefs->GetMutableList(prefs::kExtensionInstallDenyList);
    ASSERT_TRUE(blacklist != NULL);

    // Blacklist this extension.
    blacklist->Append(Value::CreateStringValue(good_crx));
    prefs->ScheduleSavePersistentPrefs();
  }

  // Extension should not be running now.
  loop_.RunAllPending();
  EXPECT_EQ(0u, service_->extensions()->size());
}

// Tests disabling extensions
TEST_F(ExtensionServiceTest, DisableExtension) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);

  const char* extension_id = good_crx;
  EXPECT_FALSE(service_->extensions()->empty());
  EXPECT_TRUE(service_->GetExtensionById(extension_id, true) != NULL);
  EXPECT_TRUE(service_->GetExtensionById(extension_id, false) != NULL);
  EXPECT_TRUE(service_->disabled_extensions()->empty());

  // Disable it.
  service_->DisableExtension(extension_id);

  EXPECT_TRUE(service_->extensions()->empty());
  EXPECT_TRUE(service_->GetExtensionById(extension_id, true) != NULL);
  EXPECT_FALSE(service_->GetExtensionById(extension_id, false) != NULL);
  EXPECT_FALSE(service_->disabled_extensions()->empty());
}

// Tests disabling all extensions (simulating --disable-extensions flag).
TEST_F(ExtensionServiceTest, DisableAllExtensions) {
  InitializeEmptyExtensionService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());

  // Disable extensions.
  service_->set_extensions_enabled(false);
  service_->ReloadExtensions();

  // There shouldn't be extensions in either list.
  EXPECT_EQ(0u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());

  // This shouldn't do anything when all extensions are disabled.
  service_->EnableExtension(good_crx);
  service_->ReloadExtensions();

  // There still shouldn't be extensions in either list.
  EXPECT_EQ(0u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());

  // And then re-enable the extensions.
  service_->set_extensions_enabled(true);
  service_->ReloadExtensions();

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());
}

// Tests reloading extensions
TEST_F(ExtensionServiceTest, ReloadExtensions) {
  InitializeEmptyExtensionService();
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // Simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);
  const char* extension_id = good_crx;
  service_->DisableExtension(extension_id);

  EXPECT_EQ(0u, service_->extensions()->size());
  EXPECT_EQ(1u, service_->disabled_extensions()->size());

  service_->ReloadExtensions();

  // Extension counts shouldn't change.
  EXPECT_EQ(0u, service_->extensions()->size());
  EXPECT_EQ(1u, service_->disabled_extensions()->size());

  service_->EnableExtension(extension_id);

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());

  // Need to clear |loaded_| manually before reloading as the
  // EnableExtension() call above inserted into it and
  // UnloadAllExtensions() doesn't send out notifications.
  loaded_.clear();
  service_->ReloadExtensions();

  // Extension counts shouldn't change.
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());
}

// Tests uninstalling normal extensions
TEST_F(ExtensionServiceTest, UninstallExtension) {
  InitializeEmptyExtensionService();
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
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Extension::INTERNAL);

  // Uninstall it.
  service_->UninstallExtension(extension_id, false);
  total_successes_ = 0;

  // We should get an unload notification.
  ASSERT_TRUE(unloaded_id_.length());
  EXPECT_EQ(extension_id, unloaded_id_);

  ValidatePrefKeyCount(0);

  // The extension should not be in the service anymore.
  ASSERT_FALSE(service_->GetExtensionById(extension_id, false));
  loop_.RunAllPending();

  // The directory should be gone.
  EXPECT_FALSE(file_util::PathExists(extension_path));
}

// Tests the uninstaller helper.
TEST_F(ExtensionServiceTest, UninstallExtensionHelper) {
  InitializeEmptyExtensionService();
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

  bool result = ExtensionService::UninstallExtensionHelper(service_,
                                                            extension_id);
  total_successes_ = 0;

  EXPECT_TRUE(result);

  // We should get an unload notification.
  ASSERT_TRUE(unloaded_id_.length());
  EXPECT_EQ(extension_id, unloaded_id_);

  ValidatePrefKeyCount(0);

  // The extension should not be in the service anymore.
  ASSERT_FALSE(service_->GetExtensionById(extension_id, false));
  loop_.RunAllPending();

  // The directory should be gone.
  EXPECT_FALSE(file_util::PathExists(extension_path));

  // Attempt to uninstall again. This should fail as we just removed the
  // extension.
  result = ExtensionService::UninstallExtensionHelper(service_, extension_id);
  EXPECT_FALSE(result);
}

// Verifies extension state is removed upon uninstall
TEST_F(ExtensionServiceTest, ClearExtensionData) {
  InitializeEmptyExtensionService();

  // Load a test extension.
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions");
  path = path.AppendASCII("good.crx");
  InstallExtension(path, true);
  const Extension* extension = service_->GetExtensionById(good_crx, false);
  ASSERT_TRUE(extension);
  GURL ext_url(extension->url());
  string16 origin_id =
      webkit_database::DatabaseUtil::GetOriginIdentifier(ext_url);

  // Set a cookie for the extension.
  net::CookieMonster* cookie_monster = profile_
      ->GetRequestContextForExtensions()->GetCookieStore()->GetCookieMonster();
  ASSERT_TRUE(cookie_monster);
  net::CookieOptions options;
  cookie_monster->SetCookieWithOptions(ext_url, "dummy=value", options);
  net::CookieList list = cookie_monster->GetAllCookiesForURL(ext_url);
  EXPECT_EQ(1U, list.size());

  // Open a database.
  webkit_database::DatabaseTracker* db_tracker = profile_->GetDatabaseTracker();
  string16 db_name = UTF8ToUTF16("db");
  string16 description = UTF8ToUTF16("db_description");
  int64 size;
  int64 available;
  db_tracker->DatabaseOpened(origin_id, db_name, description, 1, &size,
                             &available);
  db_tracker->DatabaseClosed(origin_id, db_name);
  std::vector<webkit_database::OriginInfo> origins;
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(origin_id, origins[0].GetOrigin());

  // Create local storage. We only simulate this by creating the backing file
  // since webkit is not initialized.
  DOMStorageContext* context =
      profile_->GetWebKitContext()->dom_storage_context();
  FilePath lso_path = context->GetLocalStorageFilePath(origin_id);
  EXPECT_TRUE(file_util::CreateDirectory(lso_path.DirName()));
  EXPECT_EQ(0, file_util::WriteFile(lso_path, NULL, 0));
  EXPECT_TRUE(file_util::PathExists(lso_path));

  // Create indexed db. Again, it is enough to only simulate this by creating
  // the file on the disk.
  IndexedDBContext* idb_context =
      profile_->GetWebKitContext()->indexed_db_context();
  FilePath idb_path = idb_context->GetIndexedDBFilePath(origin_id);
  EXPECT_TRUE(file_util::CreateDirectory(idb_path.DirName()));
  EXPECT_EQ(0, file_util::WriteFile(idb_path, NULL, 0));
  EXPECT_TRUE(file_util::PathExists(idb_path));

  // Uninstall the extension.
  service_->UninstallExtension(good_crx, false);
  loop_.RunAllPending();

  // Check that the cookie is gone.
  list = cookie_monster->GetAllCookiesForURL(ext_url);
  EXPECT_EQ(0U, list.size());

  // The database should have vanished as well.
  origins.clear();
  db_tracker->GetAllOriginsInfo(&origins);
  EXPECT_EQ(0U, origins.size());

  // Check that the LSO file has been removed.
  EXPECT_FALSE(file_util::PathExists(lso_path));

  // Check if the indexed db has disappeared too.
  EXPECT_FALSE(file_util::PathExists(idb_path));
}

// Tests loading single extensions (like --load-extension)
TEST_F(ExtensionServiceTest, LoadExtension) {
  InitializeEmptyExtensionService();
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

  ValidatePrefKeyCount(1);

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
TEST_F(ExtensionServiceTest, GenerateID) {
  InitializeEmptyExtensionService();

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

  ValidatePrefKeyCount(1);

  std::string previous_id = loaded_[0]->id();

  // If we reload the same path, we should get the same extension ID.
  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(previous_id, loaded_[0]->id());
}

void ExtensionServiceTest::TestExternalProvider(
    MockExtensionProvider* provider, Extension::Location location) {
  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  provider->set_visit_count(0);

  // Register a test extension externally using the mock registry provider.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  // Add the extension.
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Reloading extensions should find our externally registered extension
  // and install it.
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

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
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

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
  ValidateIntegerPref(good_crx, "state", Extension::KILLBIT);
  ValidateIntegerPref(good_crx, "location", location);

  // Now clear the preference and reinstall.
  SetPrefInteg(good_crx, "state", Extension::ENABLED);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", location);

  // Now test an externally triggered uninstall (deleting the registry key or
  // the pref entry).
  provider->RemoveExtension(good_crx);

  loaded_.clear();
  service_->OnExternalProviderReady();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(0);

  // The extension should also be gone from the install directory.
  ASSERT_FALSE(file_util::PathExists(install_path));

  // Now test the case where user uninstalls and then the extension is removed
  // from the external provider.

  provider->UpdateOrAddExtension(good_crx, "1.0", source_path);
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());

  // User uninstalls.
  loaded_.clear();
  service_->UninstallExtension(id, false);
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Then remove the extension from the extension provider.
  provider->RemoveExtension(good_crx);

  // Should still be at 0.
  loaded_.clear();
  service_->LoadAllExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(1);

  EXPECT_EQ(5, provider->visit_count());
}

// Tests the external installation feature
#if defined(OS_WIN)
TEST_F(ExtensionServiceTest, ExternalInstallRegistry) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionService();
  set_extensions_enabled(false);

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* reg_provider =
      new MockExtensionProvider(service_.get(), Extension::EXTERNAL_REGISTRY);
  AddMockExternalProvider(reg_provider);
  TestExternalProvider(reg_provider, Extension::EXTERNAL_REGISTRY);
}
#endif

TEST_F(ExtensionServiceTest, ExternalInstallPref) {
  InitializeEmptyExtensionService();

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(service_.get(), Extension::EXTERNAL_PREF);

  AddMockExternalProvider(pref_provider);
  TestExternalProvider(pref_provider, Extension::EXTERNAL_PREF);
}

TEST_F(ExtensionServiceTest, ExternalInstallPrefUpdateUrl) {
  // This should all work, even when normal extension installation is disabled.
  InitializeEmptyExtensionService();
  set_extensions_enabled(false);

  // TODO(skerner): The mock provider is not a good model of a provider
  // that works with update URLs, because it adds file and version info.
  // Extend the mock to work with update URLs.  This test checks the
  // behavior that is common to all external extension visitors.  The
  // browser test ExtensionManagementTest.ExternalUrlUpdate tests that
  // what the visitor does results in an extension being downloaded and
  // installed.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(service_.get(),
                                Extension::EXTERNAL_PREF_DOWNLOAD);
  AddMockExternalProvider(pref_provider);
  TestExternalProvider(pref_provider, Extension::EXTERNAL_PREF_DOWNLOAD);
}

// Tests that external extensions get uninstalled when the external extension
// providers can't account for them.
TEST_F(ExtensionServiceTest, ExternalUninstall) {
  // Start the extensions service with one external extension already installed.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("PreferencesExternal");

  // This initializes the extensions service with no ExternalExtensionProviders.
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  set_extensions_enabled(false);

  service_->Init();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());

  // Verify that it's not the disabled extensions flag causing it not to load.
  set_extensions_enabled(true);
  service_->ReloadExtensions();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(0u, loaded_.size());
}

// Test that running multiple update checks simultaneously does not
// keep the update from succeeding.
TEST_F(ExtensionServiceTest, MultipleExternalUpdateCheck) {
  InitializeEmptyExtensionService();

  MockExtensionProvider* provider =
      new MockExtensionProvider(service_.get(), Extension::EXTERNAL_PREF);
  AddMockExternalProvider(provider);

  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Start two checks for updates.
  provider->set_visit_count(0);
  service_->CheckForExternalUpdates();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

  // Two calls should cause two checks for external extensions.
  EXPECT_EQ(2, provider->visit_count());
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());

  // Register a test extension externally using the mock registry provider.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Two checks for external updates should find the extension, and install it
  // once.
  provider->set_visit_count(0);
  service_->CheckForExternalUpdates();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();
  EXPECT_EQ(2, provider->visit_count());
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL_PREF, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", Extension::EXTERNAL_PREF);

  provider->RemoveExtension(good_crx);
  provider->set_visit_count(0);
  service_->CheckForExternalUpdates();
  service_->CheckForExternalUpdates();
  loop_.RunAllPending();

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
  FilePath base_path(FILE_PATH_LITERAL("//base/path"));
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
      "    \"external_update_url\": \"http:\\\\foo.com/update\""
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
      "  \"pppppppppppppppppppppppppppppppp\": {"
      "    \"external_crx\": \"RandomValidExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  }"
      "}";
  EXPECT_EQ(1, visitor.Visit(json_data));

  // Check that if a base path is not provided, use of a relative
  // path fails.
  FilePath empty;
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
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAndRelocalizeExtensions) {
  // Initialize the test dir with a good Preferences/extensions.
  FilePath source_install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_install_dir));
  source_install_dir = source_install_dir
      .AppendASCII("extensions")
      .AppendASCII("l10n");
  FilePath pref_path = source_install_dir.AppendASCII("Preferences");
  InitializeInstalledExtensionService(pref_path, source_install_dir);

  service_->Init();
  loop_.RunAllPending();

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
// combinations. We don't want to derive from the ExtensionServiceTest class
// for this test, so we use ExtensionServiceTestSimple.
//
// Also tests that we always fire EXTENSIONS_READY, no matter whether we are
// enabled or not.
TEST(ExtensionServiceTestSimple, Enabledness) {
  ExtensionsReadyRecorder recorder;
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  MessageLoop loop;
  BrowserThread ui_thread(BrowserThread::UI, &loop);
  BrowserThread file_thread(BrowserThread::FILE, &loop);
  scoped_ptr<CommandLine> command_line;
  scoped_refptr<ExtensionService> service;
  FilePath install_dir = profile->GetPath()
      .AppendASCII(ExtensionService::kInstallDirectoryName);

  // By default, we are enabled.
  command_line.reset(new CommandLine(CommandLine::NO_PROGRAM));
  service = profile->CreateExtensionService(command_line.get(),
                                            install_dir);
  EXPECT_TRUE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  // If either the command line or pref is set, we are disabled.
  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  command_line->AppendSwitch(switches::kDisableExtensions);
  service = profile->CreateExtensionService(command_line.get(),
                                             install_dir);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
  service = profile->CreateExtensionService(command_line.get(),
                                            install_dir);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  recorder.set_ready(false);
  profile.reset(new TestingProfile());
  profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
  command_line.reset(new CommandLine(CommandLine::NO_PROGRAM));
  service = profile->CreateExtensionService(command_line.get(),
                                             install_dir);
  EXPECT_FALSE(service->extensions_enabled());
  service->Init();
  loop.RunAllPending();
  EXPECT_TRUE(recorder.ready());

  // Explicitly delete all the resources used in this test.
  profile.reset();
  service = NULL;
}

// Test loading extensions that require limited and unlimited storage quotas.
TEST_F(ExtensionServiceTest, StorageQuota) {
  InitializeEmptyExtensionService();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions")
      .AppendASCII("storage_quota");

  FilePath limited_quota_ext = extensions_path.AppendASCII("limited_quota")
      .AppendASCII("1.0");

  // The old permission name for unlimited quota was "unlimited_storage", but
  // we changed it to "unlimitedStorage". This tests both versions.
  FilePath unlimited_quota_ext = extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("1.0");
  FilePath unlimited_quota_ext2 = extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("2.0");
  service_->LoadExtension(limited_quota_ext);
  service_->LoadExtension(unlimited_quota_ext);
  service_->LoadExtension(unlimited_quota_ext2);
  loop_.RunAllPending();

  ASSERT_EQ(3u, loaded_.size());
  EXPECT_TRUE(profile_.get());
  EXPECT_FALSE(profile_->IsOffTheRecord());

  // Open the database from each origin to make the tracker aware
  // of the existence of these origins and to get their quotas.
  int64 limited_quota = -1;
  int64 unlimited_quota = -1;
  string16 limited_quota_identifier =
      webkit_database::DatabaseUtil::GetOriginIdentifier(loaded_[0]->url());
  string16 unlimited_quota_identifier =
      webkit_database::DatabaseUtil::GetOriginIdentifier(loaded_[1]->url());
  string16 unlimited_quota_identifier2 =
      webkit_database::DatabaseUtil::GetOriginIdentifier(loaded_[2]->url());
  string16 db_name = UTF8ToUTF16("db");
  string16 description = UTF8ToUTF16("db_description");
  int64 database_size;
  webkit_database::DatabaseTracker* db_tracker = profile_->GetDatabaseTracker();

  // First check the normal limited quota extension.
  db_tracker->DatabaseOpened(limited_quota_identifier, db_name, description,
                             1, &database_size, &limited_quota);
  db_tracker->DatabaseClosed(limited_quota_identifier, db_name);
  EXPECT_EQ(profile_->GetDatabaseTracker()->GetDefaultQuota(), limited_quota);

  // Now check the two unlimited quota ones.
  db_tracker->DatabaseOpened(unlimited_quota_identifier, db_name, description,
                             1, &database_size, &unlimited_quota);
  db_tracker->DatabaseClosed(unlimited_quota_identifier, db_name);
  EXPECT_EQ(kint64max, unlimited_quota);
  db_tracker->DatabaseOpened(unlimited_quota_identifier2, db_name, description,
                             1, &database_size, &unlimited_quota);
  db_tracker->DatabaseClosed(unlimited_quota_identifier2, db_name);

  EXPECT_EQ(kint64max, unlimited_quota);
}

// Tests ExtensionService::register_component_extension().
TEST_F(ExtensionServiceTest, ComponentExtensions) {
  InitializeEmptyExtensionService();

  // Component extensions should work even when extensions are disabled.
  set_extensions_enabled(false);

  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::string manifest;
  ASSERT_TRUE(file_util::ReadFileToString(
      path.Append(Extension::kManifestFilename), &manifest));

  service_->register_component_extension(
      ExtensionService::ComponentExtensionInfo(manifest, path));
  service_->Init();

  // Note that we do not pump messages -- the extension should be loaded
  // immediately.

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Extension::COMPONENT, loaded_[0]->location());
  EXPECT_EQ(1u, service_->extensions()->size());

  // Component extensions shouldn't get recourded in the prefs.
  ValidatePrefKeyCount(0);

  // Reload all extensions, and make sure it comes back.
  std::string extension_id = service_->extensions()->at(0)->id();
  loaded_.clear();
  service_->ReloadExtensions();
  ASSERT_EQ(1u, service_->extensions()->size());
  EXPECT_EQ(extension_id, service_->extensions()->at(0)->id());
}
