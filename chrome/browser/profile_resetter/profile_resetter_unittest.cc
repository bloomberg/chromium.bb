// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using extensions::Extension;
using extensions::Manifest;

class MockObject {
 public:
  void RunLoop() {
    EXPECT_CALL(*this, Callback());
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

  void StopLoop() {
    DCHECK(runner_.get());
    Callback();
    runner_->Quit();
  }

 private:
  MOCK_METHOD0(Callback, void(void));

  scoped_refptr<content::MessageLoopRunner> runner_;
};

class ProfileResetterTest : public testing::Test {
 protected:
  ProfileResetterTest();
  ~ProfileResetterTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  testing::StrictMock<MockObject> mock_object_;
  TemplateURLServiceTestUtil test_util_;
  scoped_ptr<ProfileResetter> resetter_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetterTest);
};

ProfileResetterTest::ProfileResetterTest() {}

ProfileResetterTest::~ProfileResetterTest() {}

void ProfileResetterTest::SetUp() {
  test_util_.SetUp();
  resetter_.reset(new ProfileResetter(test_util_.profile()));
}

void ProfileResetterTest::TearDown() {
  test_util_.TearDown();
}

// ExtensionsResetTest sets up the extension service.
class ExtensionsResetTest : public ExtensionServiceTestBase {
 protected:
  virtual void SetUp() OVERRIDE;

  testing::StrictMock<MockObject> mock_object_;
  scoped_ptr<ProfileResetter> resetter_;
};

void ExtensionsResetTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  resetter_.reset(new ProfileResetter(profile_.get()));
}

// Returns a barebones test Extension object with the specified |name|.  The
// returned extension will include background permission iff
// |background_permission| is true.
scoped_refptr<Extension> CreateExtension(const std::string& name,
                                         const base::FilePath& path,
                                         Manifest::Location location,
                                         bool theme) {
  DictionaryValue manifest;
  manifest.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extension_manifest_keys::kName, name);
  if (theme)
    manifest.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path,
      location,
      manifest,
      Extension::NO_FLAGS,
      &error);
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
}


TEST_F(ProfileResetterTest, ResetDefaultSearchEngine) {
  resetter_->Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetHomepage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  resetter_->Reset(
      ProfileResetter::HOMEPAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(std::string(), prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  resetter_->Reset(
      ProfileResetter::CONTENT_SETTINGS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetCookiesAndSiteData) {
  resetter_->Reset(
      ProfileResetter::COOKIES_AND_SITE_DATA,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ExtensionsResetTest, ResetExtensionsByDisabling) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> theme = CreateExtension("example1", temp_dir.path(),
                                                   Manifest::INVALID_LOCATION,
                                                   true);
  service_->FinishInstallationForTest(theme.get());
  // Let ThemeService finish creating the theme pack.
  base::MessageLoop::current()->RunUntilIdle();

#if !defined(OS_ANDROID)
  // ThemeService isn't compiled for Android.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
#endif

  scoped_refptr<Extension> ext2 = CreateExtension(
      "example2",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      false);
  service_->AddExtension(ext2);
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      "example3",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::COMPONENT,
      false);
  service_->AddExtension(ext3);
  scoped_refptr<Extension> ext4 = CreateExtension(
      "example4",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
      Manifest::EXTERNAL_POLICY_DOWNLOAD,
      false);
  service_->AddExtension(ext4);
  EXPECT_EQ(4u, service_->extensions()->size());

  resetter_->Reset(
      ProfileResetter::EXTENSIONS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_EQ(2u, service_->extensions()->size());
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext4->id()));
#if !defined(OS_ANDROID)
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
#endif
}

TEST_F(ProfileResetterTest, ResetStartPage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL("http://foo"));
  startup_pref.urls.push_back(GURL("http://bar"));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  resetter_->Reset(
      ProfileResetter::STARTUP_PAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(), startup_pref.urls);
}

TEST_F(ProfileResetterTest, ResetFewFlags) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  resetter_->Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE | ProfileResetter::HOMEPAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

}  // namespace
