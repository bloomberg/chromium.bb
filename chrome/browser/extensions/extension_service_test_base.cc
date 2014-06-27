// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_base.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/install_limiter.h"
#endif

namespace extensions {

namespace {

// Create a testing profile according to |params|.
scoped_ptr<TestingProfile> BuildTestingProfile(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  TestingProfile::Builder profile_builder;
  // Create a PrefService that only contains user defined preference values.
  PrefServiceMockFactory factory;
  // If pref_file is empty, TestingProfile automatically creates
  // TestingPrefServiceSyncable instance.
  if (!params.pref_file.empty()) {
    factory.SetUserPrefsFile(params.pref_file,
                             base::MessageLoopProxy::current().get());
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    scoped_ptr<PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    chrome::RegisterUserProfilePrefs(registry.get());
    profile_builder.SetPrefService(prefs.Pass());
  }

  if (params.profile_is_supervised)
    profile_builder.SetSupervisedUserId("asdf");

  profile_builder.SetPath(params.profile_path);
  return profile_builder.Build();
}

}  // namespace

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ExtensionServiceInitParams()
    : autoupdate_enabled(false),
      is_first_run(true),
      profile_is_supervised(false) {
}

// Our message loop may be used in tests which require it to be an IO loop.
ExtensionServiceTestBase::ExtensionServiceTestBase()
    : service_(NULL),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      registry_(NULL) {
  base::FilePath test_data_dir;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  data_dir_ = test_data_dir.AppendASCII("extensions");
}

ExtensionServiceTestBase::~ExtensionServiceTestBase() {
  // Why? Because |profile_| has to be destroyed before |at_exit_manager_|, but
  // is declared above it in the class definition since it's protected.
  profile_.reset();
}

ExtensionServiceTestBase::ExtensionServiceInitParams
ExtensionServiceTestBase::CreateDefaultInitParams() {
  ExtensionServiceInitParams params;
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.path();
  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  EXPECT_TRUE(base::DeleteFile(path, true));
  base::File::Error error = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;
  base::FilePath prefs_filename =
      path.Append(FILE_PATH_LITERAL("TestPreferences"));
  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  EXPECT_TRUE(base::DeleteFile(extensions_install_dir, true));
  EXPECT_TRUE(base::CreateDirectoryAndGetError(extensions_install_dir, &error))
      << error;

  params.profile_path = path;
  params.pref_file = prefs_filename;
  params.extensions_install_dir = extensions_install_dir;
  return params;
}

void ExtensionServiceTestBase::InitializeExtensionService(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  profile_ = BuildTestingProfile(params);
  CreateExtensionService(params);

  extensions_install_dir_ = params.extensions_install_dir;
  registry_ = ExtensionRegistry::Get(profile_.get());

  // Garbage collector is typically NULL during tests, so give it a build.
  ExtensionGarbageCollectorFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), &ExtensionGarbageCollectorFactory::BuildInstanceFor);
}

void ExtensionServiceTestBase::InitializeEmptyExtensionService() {
  InitializeExtensionService(CreateDefaultInitParams());
}

void ExtensionServiceTestBase::InitializeInstalledExtensionService(
    const base::FilePath& prefs_file,
    const base::FilePath& source_install_dir) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.path();

  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  ASSERT_TRUE(base::DeleteFile(path, true));

  base::File::Error error = base::File::FILE_OK;
  ASSERT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;

  base::FilePath temp_prefs = path.Append(chrome::kPreferencesFilename);
  ASSERT_TRUE(base::CopyFile(prefs_file, temp_prefs));

  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  ASSERT_TRUE(base::DeleteFile(extensions_install_dir, true));
  ASSERT_TRUE(
      base::CopyDirectory(source_install_dir, extensions_install_dir, true));

  ExtensionServiceInitParams params;
  params.profile_path = path;
  params.pref_file = temp_prefs;
  params.extensions_install_dir = extensions_install_dir;
  InitializeExtensionService(params);
}

void ExtensionServiceTestBase::InitializeGoodInstalledExtensionService() {
  base::FilePath source_install_dir =
      data_dir_.AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);
  InitializeInstalledExtensionService(pref_path, source_install_dir);
}

void ExtensionServiceTestBase::InitializeExtensionServiceWithUpdater() {
  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.autoupdate_enabled = true;
  InitializeExtensionService(params);
  // TODO(thestig): Remove this once we disable all tests that use
  // ExtensionServiceTestBase.
#if defined(ENABLE_EXTENSIONS)
  service_->updater()->Start();
#endif
}

void ExtensionServiceTestBase::InitializeProcessManager() {
  static_cast<extensions::TestExtensionSystem*>(
      ExtensionSystem::Get(profile_.get()))->CreateProcessManager();
}

void ExtensionServiceTestBase::SetUp() {
  ExtensionErrorReporter::GetInstance()->ClearErrors();
}

void ExtensionServiceTestBase::SetUpTestCase() {
  // Safe to call multiple times.
  ExtensionErrorReporter::Init(false);  // no noisy errors.
}

// These are declared in the .cc so that all inheritors don't need to know
// that TestingProfile derives Profile derives BrowserContext.
content::BrowserContext* ExtensionServiceTestBase::browser_context() {
  return profile_.get();
}

Profile* ExtensionServiceTestBase::profile() {
  return profile_.get();
}

void ExtensionServiceTestBase::CreateExtensionService(
    const ExtensionServiceInitParams& params) {
  TestExtensionSystem* system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
  if (!params.is_first_run) {
    ExtensionPrefs* prefs = system->CreateExtensionPrefs(
        base::CommandLine::ForCurrentProcess(), params.extensions_install_dir);
    prefs->SetAlertSystemFirstRun();
  }

  service_ =
      system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                     params.extensions_install_dir,
                                     params.autoupdate_enabled);

  service_->SetFileTaskRunnerForTesting(
      base::MessageLoopProxy::current().get());
  service_->set_extensions_enabled(true);
  service_->set_show_extensions_prompts(false);
  service_->set_install_updates_when_idle_for_test(false);

  // When we start up, we want to make sure there is no external provider,
  // since the ExtensionService on Windows will use the Registry as a default
  // provider and if there is something already registered there then it will
  // interfere with the tests. Those tests that need an external provider
  // will register one specifically.
  service_->ClearProvidersForTesting();

#if defined(OS_CHROMEOS)
  InstallLimiter::Get(profile_.get())->DisableForTest();
#endif
}

}  // namespace extensions
