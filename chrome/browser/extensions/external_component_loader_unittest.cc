// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;
using extensions::ExternalProviderImpl;
using extensions::ExternalProviderInterface;
using extensions::Manifest;
using extensions::ProviderCollection;

namespace extensions {

namespace {
class TestUtterance : public Utterance {
 public:
  explicit TestUtterance(Profile* profile) : Utterance(profile) {
  }

  virtual ~TestUtterance() {
    set_finished_for_testing(true);
  }
};

class FakeVisitorInterface
    : public ExternalProviderInterface::VisitorInterface {
 public:
  FakeVisitorInterface() {}
  virtual ~FakeVisitorInterface() {}

  virtual bool OnExternalExtensionFileFound(
      const std::string& id,
      const base::Version* version,
      const base::FilePath& path,
      Manifest::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE {
    return true;
  }

  virtual bool OnExternalExtensionUpdateUrlFound(
      const std::string& id,
      const GURL& update_url,
      Manifest::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE {
    return true;
  }

  virtual void OnExternalProviderReady(
      const ExternalProviderInterface* provider) OVERRIDE {}
};
}  // anonymous namespace

#if defined(OS_CHROMEOS)
class ExternalComponentLoaderTest : public testing::Test {
 public:
  ExternalComponentLoaderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        user_manager_enabler_(new chromeos::UserManagerImpl()) {
  }

  virtual ~ExternalComponentLoaderTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    testing_profile_.reset(new TestingProfile());
    ExternalProviderImpl::CreateExternalProviders(
        &service_, testing_profile_.get(), &providers_);
  }

  virtual void TearDown() OVERRIDE {
  }

  bool IsHighQualityEnglishSpeechExtensionInstalled() {
    const std::string& id = extension_misc::kHighQuality_en_US_ExtensionId;
    for (size_t i = 0; i < providers_.size(); ++i) {
      if (!providers_[i]->IsReady())
        continue;
      if (providers_[i]->HasExtension(id))
        return true;
    }
    return false;
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  scoped_ptr<Profile> testing_profile_;
  FakeVisitorInterface service_;
  ProviderCollection providers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalComponentLoaderTest);
};

TEST_F(ExternalComponentLoaderTest, Speaking100TimesInstallsSpeechExtension) {
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());

  TtsPlatformImpl* tts_platform = TtsPlatformImpl::GetInstance();
  TestUtterance utterance(testing_profile_.get());
  VoiceData voice_data;
  voice_data.lang = "en-US";
  voice_data.extension_id = extension_misc::kSpeechSynthesisExtensionId;

  // 99 times should not be sufficient.
  for (int i = 0; i < 99; i++)
    tts_platform->WillSpeakUtteranceWithVoice(&utterance, voice_data);
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());

  // The 100th time should install it.
  tts_platform->WillSpeakUtteranceWithVoice(&utterance, voice_data);
  ASSERT_TRUE(IsHighQualityEnglishSpeechExtensionInstalled());
}

TEST_F(ExternalComponentLoaderTest,
       UsingOtherVoiceDoesNotTriggerInstallingSpeechExtension) {
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());

  TtsPlatformImpl* tts_platform = TtsPlatformImpl::GetInstance();
  TestUtterance utterance(testing_profile_.get());
  VoiceData voice_data;
  voice_data.lang = "en-US";
  voice_data.extension_id = "dummy";  // Some other extension id.

  for (int i = 0; i < 100; i++)
    tts_platform->WillSpeakUtteranceWithVoice(&utterance, voice_data);
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());
}

TEST_F(ExternalComponentLoaderTest,
       UnsupportedLangDoesNotTriggerInstallingSpeechExtension) {
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());

  TtsPlatformImpl* tts_platform = TtsPlatformImpl::GetInstance();
  TestUtterance utterance(testing_profile_.get());
  VoiceData voice_data;
  voice_data.lang = "tlh";  // Klingon
  voice_data.extension_id = extension_misc::kSpeechSynthesisExtensionId;

  for (int i = 0; i < 100; i++)
    tts_platform->WillSpeakUtteranceWithVoice(&utterance, voice_data);
  ASSERT_FALSE(IsHighQualityEnglishSpeechExtensionInstalled());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
