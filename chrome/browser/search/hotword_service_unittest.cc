// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/one_shot_event.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

namespace {

class MockAudioHistoryHandler : public HotwordAudioHistoryHandler {
 public:
  MockAudioHistoryHandler(
      content::BrowserContext* context,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      history::WebHistoryService* web_history)
      : HotwordAudioHistoryHandler(context, task_runner),
        get_audio_history_calls_(0),
        web_history_(web_history) {
  }
  ~MockAudioHistoryHandler() override {}

  void GetAudioHistoryEnabled(
      const HotwordAudioHistoryCallback& callback) override {
    get_audio_history_calls_++;
    callback.Run(true, true);
  }

  history::WebHistoryService* GetWebHistory() override {
    return web_history_.get();
  }

  int GetAudioHistoryCalls() {
    return get_audio_history_calls_;
  }

 private:
  int get_audio_history_calls_;
  std::unique_ptr<history::WebHistoryService> web_history_;
};

class MockHotwordService : public HotwordService {
 public:
  explicit MockHotwordService(Profile* profile)
      : HotwordService(profile),
        uninstall_count_(0) {
  }

  bool UninstallHotwordExtension(ExtensionService* extension_service) override {
    uninstall_count_++;
    return HotwordService::UninstallHotwordExtension(extension_service);
  }

  void InstallHotwordExtensionFromWebstore(int num_tries) override {
    std::unique_ptr<base::DictionaryValue> manifest =
        extensions::DictionaryBuilder()
            .Set("name", "Hotword Test Extension")
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Build();
    scoped_refptr<extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .AddFlags(extensions::Extension::FROM_WEBSTORE |
                      extensions::Extension::WAS_INSTALLED_BY_DEFAULT)
            .SetID(extension_id_)
            .SetLocation(extensions::Manifest::EXTERNAL_COMPONENT)
            .Build();
    ASSERT_TRUE(extension.get());
    service_->OnExtensionInstalled(extension.get(), syncer::StringOrdinal());
  }


  int uninstall_count() { return uninstall_count_; }

  void SetExtensionService(ExtensionService* service) { service_ = service; }
  void SetExtensionId(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  ExtensionService* extension_service() { return service_; }

 private:
  ExtensionService* service_;
  int uninstall_count_;
  std::string extension_id_;
};

std::unique_ptr<KeyedService> BuildMockHotwordService(
    content::BrowserContext* context) {
  return base::MakeUnique<MockHotwordService>(static_cast<Profile*>(context));
}

}  // namespace

class HotwordServiceTest :
  public extensions::ExtensionServiceTestBase,
  public ::testing::WithParamInterface<const char*> {
 protected:
  HotwordServiceTest() {}
  virtual ~HotwordServiceTest() {}

  void SetApplicationLocale(Profile* profile, const std::string& new_locale) {
#if defined(OS_CHROMEOS)
        // On ChromeOS locale is per-profile.
    profile->GetPrefs()->SetString(prefs::kApplicationLocale, new_locale);
#else
    g_browser_process->SetApplicationLocale(new_locale);
#endif
  }

  void SetUp() override {
    extension_id_ = GetParam();
#if defined(OS_CHROMEOS)
    // Tests on chromeos need to have the handler initialized.
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif

    extensions::ExtensionServiceTestBase::SetUp();
  }

  void TearDown() override {
#if defined(OS_CHROMEOS)
    DCHECK(chromeos::CrasAudioHandler::IsInitialized());
    chromeos::CrasAudioHandler::Shutdown();
#endif
  }

  std::string extension_id_;
};

INSTANTIATE_TEST_CASE_P(HotwordServiceTests,
                        HotwordServiceTest,
                        ::testing::Values(
                            extension_misc::kHotwordSharedModuleId));

// Disabled due to http://crbug.com/503963.
TEST_P(HotwordServiceTest, DISABLED_IsHotwordAllowedLocale) {
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();

#if defined(ENABLE_HOTWORDING)
  bool hotwording_enabled = true;
#else
  bool hotwording_enabled = false;
#endif

  // Check that the service exists so that a NULL service be ruled out in
  // following tests.
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(profile.get());
  EXPECT_TRUE(hotword_service != NULL);

  // Set the language to an invalid one.
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "non-valid");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Now with valid locales it should be fine.
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en");
  EXPECT_EQ(hotwording_enabled,
            HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en-US");
  EXPECT_EQ(hotwording_enabled,
            HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en_us");
  EXPECT_EQ(hotwording_enabled,
            HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "de_DE");
  EXPECT_EQ(hotwording_enabled,
            HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "fr_fr");
  EXPECT_EQ(hotwording_enabled,
            HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Test that incognito even with a valid locale and valid field trial
  // still returns false.
  Profile* otr_profile = profile->GetOffTheRecordProfile();
  SetApplicationLocale(otr_profile, "en");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(otr_profile));
}

TEST_P(HotwordServiceTest, ShouldReinstallExtension) {
  InitializeEmptyExtensionService();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionId(extension_id_);

  // If no locale has been set, no reason to uninstall.
  EXPECT_FALSE(hotword_service->ShouldReinstallHotwordExtension());

  SetApplicationLocale(profile(), "en");
  hotword_service->SetPreviousLanguagePref();

  // Now a locale is set, but it hasn't changed.
  EXPECT_FALSE(hotword_service->ShouldReinstallHotwordExtension());

  SetApplicationLocale(profile(), "fr_fr");

  // Now it's a different locale so it should uninstall.
  EXPECT_TRUE(hotword_service->ShouldReinstallHotwordExtension());
}

TEST_P(HotwordServiceTest, PreviousLanguageSetOnInstall) {
  InitializeEmptyExtensionService();
  service_->Init();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  // If no locale has been set, no reason to uninstall.
  EXPECT_FALSE(hotword_service->ShouldReinstallHotwordExtension());

  SetApplicationLocale(profile(), "test_locale");

  hotword_service->InstallHotwordExtensionFromWebstore(1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("test_locale",
            profile()->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage));
}

TEST_P(HotwordServiceTest, UninstallReinstallTriggeredCorrectly) {
  InitializeEmptyExtensionService();
  service_->Init();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  // Initialize the locale to "en".
  SetApplicationLocale(profile(), "en");

  // The previous locale should not be set. No reason to uninstall.
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());

  // Do an initial installation.
  hotword_service->InstallHotwordExtensionFromWebstore(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("en",
            profile()->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage));

  if (extension_id_ == extension_misc::kHotwordSharedModuleId) {
    // Shared module is installed and enabled.
    EXPECT_EQ(0U, registry()->disabled_extensions().size());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id_));
  } else {
    // Verify the extension is installed but disabled.
    EXPECT_EQ(1U, registry()->disabled_extensions().size());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id_));
  }

  // The previous locale should be set but should match the current
  // locale. No reason to uninstall.
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());

  // Switch the locale to a valid but different one.
  SetApplicationLocale(profile(), "fr_fr");
#if defined(ENABLE_HOTWORDING)
  EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile()));
#else
  // Disabled due to http://crbug.com/503963.
  // EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile()));
#endif

  // Different but valid locale so expect uninstall.
  EXPECT_TRUE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_EQ(1, hotword_service->uninstall_count());
  EXPECT_EQ("fr_fr",
            profile()->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage));

  if (extension_id_ == extension_misc::kHotwordSharedModuleId) {
    // Shared module is installed and enabled.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id_));
  } else {
    // Verify the extension is installed. It's still disabled.
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id_));
  }

  // Switch the locale to an invalid one.
  SetApplicationLocale(profile(), "invalid");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile()));
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_EQ("fr_fr",
            profile()->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage));

  // If the locale is set back to the last valid one, then an uninstall-install
  // shouldn't be needed.
  SetApplicationLocale(profile(), "fr_fr");
#if defined(ENABLE_HOTWORDING)
  EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile()));
#else
  // Disabled due to http://crbug.com/503963.
  // EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile()));
#endif
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_EQ(1, hotword_service->uninstall_count());  // no change
}

TEST_P(HotwordServiceTest, DisableAlwaysOnOnLanguageChange) {
  // Bypass test for old hotwording.
  if (extension_id_ != extension_misc::kHotwordSharedModuleId)
    return;

  // Turn on Always On
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalHotwordHardware);

  InitializeEmptyExtensionService();
  service_->Init();

  // Enable always-on.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  // Initialize the locale to "en_us".
  SetApplicationLocale(profile(), "en_us");

  // The previous locale should not be set. No reason to uninstall.
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_TRUE(hotword_service->IsAlwaysOnEnabled());

  // Do an initial installation.
  hotword_service->InstallHotwordExtensionFromWebstore(1);
  base::RunLoop().RunUntilIdle();

  // The previous locale should be set but should match the current
  // locale. No reason to uninstall.
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_TRUE(hotword_service->IsAlwaysOnEnabled());

  // TODO(kcarattini): Uncomment this sectione once we launch always-on
  // in more languages.
  // // Switch the locale to a valid but different one.
  // SetApplicationLocale(profile(), "fr_fr");
  // EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile()));

  // // Different but valid locale so expect uninstall.
  // EXPECT_TRUE(hotword_service->MaybeReinstallHotwordExtension());
  // EXPECT_FALSE(hotword_service->IsAlwaysOnEnabled());

  // Re-enable always-on.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);

  // Switch the locale to an invalid one.
  SetApplicationLocale(profile(), "invalid");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile()));
  EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  EXPECT_TRUE(hotword_service->IsAlwaysOnEnabled());

  // TODO(kcarattini): Uncomment this sectione once we launch always-on
  // in more languages.
  // // If the locale is set back to the last valid one, then an
  // // uninstall-install shouldn't be needed.
  // SetApplicationLocale(profile(), "fr_fr");
  // EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile()));
  // EXPECT_FALSE(hotword_service->MaybeReinstallHotwordExtension());
  // EXPECT_TRUE(hotword_service->IsAlwaysOnEnabled());
}

TEST_P(HotwordServiceTest, IsAlwaysOnEnabled) {
  // Bypass test for old hotwording.
  if (extension_id_ != extension_misc::kHotwordSharedModuleId)
    return;

  InitializeEmptyExtensionService();
  service_->Init();
  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  // No hardware available. Should never be true.
  EXPECT_FALSE(hotword_service->IsAlwaysOnEnabled());

  // Enable always-on, still not available.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);
  EXPECT_FALSE(hotword_service->IsAlwaysOnEnabled());

  // Enable regular hotwording, still not available.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_FALSE(hotword_service->IsAlwaysOnEnabled());

  // Bypass hardware check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalHotwordHardware);
  EXPECT_TRUE(hotword_service->IsAlwaysOnEnabled());

  // Disable.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled,
                                    false);
  EXPECT_FALSE(hotword_service->IsAlwaysOnEnabled());
}

TEST_P(HotwordServiceTest, IsSometimesOnEnabled) {
  InitializeEmptyExtensionService();
  service_->Init();
  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  // No pref set.
  EXPECT_FALSE(hotword_service->IsSometimesOnEnabled());

  // Set pref.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_TRUE(hotword_service->IsSometimesOnEnabled());

  // Turn on always-on pref. Should have no effect.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);
  EXPECT_TRUE(hotword_service->IsSometimesOnEnabled());

  // Disable.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, false);
  EXPECT_FALSE(hotword_service->IsSometimesOnEnabled());

  // Bypass rest of test for old hotwording.
  if (extension_id_ != extension_misc::kHotwordSharedModuleId)
    return;

  // Bypass hardware check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalHotwordHardware);

  // With hardware, only always-on is allowed.
  EXPECT_FALSE(hotword_service->IsSometimesOnEnabled());

  // Set pref.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_FALSE(hotword_service->IsSometimesOnEnabled());

  // Disable always-on.
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled,
                                    false);
  EXPECT_FALSE(hotword_service->IsSometimesOnEnabled());
}

TEST_P(HotwordServiceTest, AudioHistorySyncOccurs) {
  InitializeEmptyExtensionService();
  service_->Init();

  HotwordServiceFactory* hotword_service_factory =
    HotwordServiceFactory::GetInstance();

  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
    hotword_service_factory->SetTestingFactoryAndUse(
    profile(), BuildMockHotwordService));
  ASSERT_TRUE(hotword_service != NULL);
  hotword_service->SetExtensionService(service());
  hotword_service->SetExtensionId(extension_id_);

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner(
      new base::TestSimpleTaskRunner());
  MockAudioHistoryHandler* audio_history_handler =
      new MockAudioHistoryHandler(profile(), test_task_runner, nullptr);
  hotword_service->SetAudioHistoryHandler(audio_history_handler);
  EXPECT_EQ(1, audio_history_handler->GetAudioHistoryCalls());
  // We expect the next check for audio history to be in the queue.
  EXPECT_EQ(base::TimeDelta::FromDays(1),
            test_task_runner->NextPendingTaskDelay());
  EXPECT_TRUE(test_task_runner->HasPendingTask());
  test_task_runner->RunPendingTasks();
  EXPECT_EQ(2, audio_history_handler->GetAudioHistoryCalls());
  EXPECT_TRUE(test_task_runner->HasPendingTask());
  test_task_runner->ClearPendingTasks();
}
