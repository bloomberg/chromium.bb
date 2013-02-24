// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_syncable_service.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/theme_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace {

static const char kCustomThemeName[] = "name";
static const char kCustomThemeUrl[] = "http://update.url/foo";

#if defined(OS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif defined(OS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/oo");
#endif

class FakeSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  FakeSyncChangeProcessor() : change_output_(NULL) {}

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
        const tracked_objects::Location& from_here,
        const syncer::SyncChangeList& change_list) OVERRIDE {
    change_output_->insert(change_output_->end(), change_list.begin(),
                           change_list.end());
    return syncer::SyncError();
  }

  void SetChangeOutput(syncer::SyncChangeList *change_output) {
    change_output_ = change_output;
  }

 private:
  syncer::SyncChangeList *change_output_;
};

class FakeThemeService : public ThemeService {
 public:
  FakeThemeService() :
    using_native_theme_(false),
    using_default_theme_(false),
    theme_extension_(NULL),
    is_dirty_(false) {}

  // ThemeService implementation
  virtual void SetTheme(const extensions::Extension* extension) OVERRIDE {
    is_dirty_ = true;
    theme_extension_ = extension;
    using_native_theme_ = false;
    using_default_theme_ = false;
  }

  virtual void UseDefaultTheme() OVERRIDE {
    is_dirty_ = true;
    using_default_theme_ = true;
    using_native_theme_ = false;
    theme_extension_ = NULL;
  }

  virtual void SetNativeTheme() OVERRIDE {
    is_dirty_ = true;
    using_native_theme_ = true;
    using_default_theme_ = false;
    theme_extension_ = NULL;
  }

  virtual bool UsingDefaultTheme() const OVERRIDE {
    return using_default_theme_;
  }

  virtual bool UsingNativeTheme() const OVERRIDE {
    return using_native_theme_;
  }

  virtual string GetThemeID() const OVERRIDE {
    if (theme_extension_)
      return theme_extension_->id();
    else
      return "";
  }

  const extensions::Extension* theme_extension() const {
    return theme_extension_.get();
  }

  bool is_dirty() const {
    return is_dirty_;
  }

  void MarkClean() {
    is_dirty_ = false;
  }

 private:
  bool using_native_theme_;
  bool using_default_theme_;
  scoped_refptr<const extensions::Extension> theme_extension_;
  bool is_dirty_;
};

ProfileKeyedService* BuildMockThemeService(Profile* profile) {
  return new FakeThemeService;
}

scoped_refptr<extensions::Extension> MakeThemeExtension(
    const base::FilePath& extension_path,
    const string& name,
    const string& update_url) {
  DictionaryValue source;
  source.SetString(extension_manifest_keys::kName, name);
  source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  source.SetString(extension_manifest_keys::kUpdateURL, update_url);
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  string error;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          extension_path, extensions::Manifest::EXTERNAL_PREF_DOWNLOAD, source,
          extensions::Extension::NO_FLAGS, &error);
  EXPECT_TRUE(extension);
  EXPECT_EQ("", error);
  return extension;
}

}  // namespace

class ThemeSyncableServiceTest : public testing::Test {
 protected:
  ThemeSyncableServiceTest()
      : loop_(MessageLoop::TYPE_DEFAULT),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        fake_theme_service_(NULL) {}

  virtual ~ThemeSyncableServiceTest() {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile);
    fake_theme_service_ = BuildForProfile(profile_.get());
    theme_sync_service_.reset(new ThemeSyncableService(profile_.get(),
                                                       fake_theme_service_));
    fake_change_processor_.reset(new FakeSyncChangeProcessor);
    SetUpExtension();
  }

  virtual void TearDown() {
    profile_.reset();
    loop_.RunUntilIdle();
  }

  void SetUpExtension() {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extensions::TestExtensionSystem* test_ext_system =
        static_cast<extensions::TestExtensionSystem*>(
                extensions::ExtensionSystem::Get(profile_.get()));
    ExtensionService* service = test_ext_system->CreateExtensionService(
        &command_line, base::FilePath(kExtensionFilePath), false);
    EXPECT_TRUE(service->extensions_enabled());
    service->Init();
    loop_.RunUntilIdle();

    // Create and add custom theme extension so the ThemeSyncableService can
    // find it.
    theme_extension_ = MakeThemeExtension(base::FilePath(kExtensionFilePath),
                                          kCustomThemeName, kCustomThemeUrl);
    service->AddExtension(theme_extension_);
    ASSERT_EQ(1u, service->extensions()->size());
  }

  FakeThemeService* BuildForProfile(Profile* profile) {
    return static_cast<FakeThemeService*>(
        ThemeServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, &BuildMockThemeService));
  }

  syncer::SyncDataList MakeThemeDataList(
      const sync_pb::ThemeSpecifics& theme_specifics) {
    syncer::SyncDataList list;
    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_theme()->CopyFrom(theme_specifics);
    list.push_back(syncer::SyncData::CreateLocalData(
        ThemeSyncableService::kCurrentThemeClientTag,
        ThemeSyncableService::kCurrentThemeNodeTitle,
        entity_specifics));
    return list;
  }

  // Needed for setting up extension service.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  FakeThemeService* fake_theme_service_;
  scoped_refptr<extensions::Extension> theme_extension_;
  scoped_ptr<ThemeSyncableService> theme_sync_service_;
  scoped_ptr<syncer::SyncChangeProcessor> fake_change_processor_;
};

TEST_F(ThemeSyncableServiceTest, AreThemeSpecificsEqual) {
  sync_pb::ThemeSpecifics a, b;
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  // Custom vs. non-custom.

  a.set_use_custom_theme(true);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  // Custom theme equality.

  b.set_use_custom_theme(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  a.set_custom_theme_id("id");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  b.set_custom_theme_id("id");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  a.set_custom_theme_update_url("http://update.url");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  a.set_custom_theme_name("name");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  // Non-custom theme equality.

  a.set_use_custom_theme(false);
  b.set_use_custom_theme(false);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  a.set_use_system_theme_by_default(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));

  b.set_use_system_theme_by_default(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEqual(a, b, true));
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeDefaultTheme) {
  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());

  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_TRUE(fake_theme_service_->UsingDefaultTheme());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeSystemTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);

  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_TRUE(fake_theme_service_->UsingNativeTheme());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeCustomTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_name(kCustomThemeUrl);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
}

TEST_F(ThemeSyncableServiceTest, DontResetThemeWhenSpecificsAreEqual) {
  // Set up theme service to use default theme and expect no changes.
  fake_theme_service_->UseDefaultTheme();
  fake_theme_service_->MarkClean();
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_FALSE(fake_theme_service_->is_dirty());
}

TEST_F(ThemeSyncableServiceTest, UpdateThemeSpecificsFromCurrentTheme) {
  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());

  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, syncer::SyncDataList(), fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();

  ASSERT_EQ(1u, change_list.size());
  EXPECT_TRUE(change_list[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change_list[0].change_type());
  EXPECT_EQ(syncer::THEMES, change_list[0].sync_data().GetDataType());

  const sync_pb::ThemeSpecifics& theme_specifics =
      change_list[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(theme_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension_->id(), theme_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension_->name(), theme_specifics.custom_theme_name());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(theme_extension_).spec(),
            theme_specifics.custom_theme_update_url());
}

TEST_F(ThemeSyncableServiceTest, GetAllSyncData) {
  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());

  syncer::SyncDataList data_list =
      theme_sync_service_->GetAllSyncData(syncer::THEMES);

  ASSERT_EQ(1u, data_list.size());
  const sync_pb::ThemeSpecifics& theme_specifics =
      data_list[0].GetSpecifics().theme();
  EXPECT_TRUE(theme_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension_->id(), theme_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension_->name(), theme_specifics.custom_theme_name());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(theme_extension_).spec(),
            theme_specifics.custom_theme_update_url());
}

TEST_F(ThemeSyncableServiceTest, ProcessSyncThemeChange) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  fake_theme_service_->MarkClean();

  // Start syncing.
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  // Don't expect theme change initially because specifics are equal.
  EXPECT_FALSE(fake_theme_service_->is_dirty());

  // Change specifics to use custom theme and update.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_name(kCustomThemeUrl);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme()->CopyFrom(theme_specifics);
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(FROM_HERE,
                                           syncer::SyncChange::ACTION_UPDATE,
                                           syncer::SyncData::CreateRemoteData(
                                               1, entity_specifics)));
  error = theme_sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
}

TEST_F(ThemeSyncableServiceTest, OnThemeChangeByUser) {
  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Start syncing.
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_EQ(0u, change_list.size());

  // Change current theme to custom theme and notify theme_sync_service_.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(1u, change_list.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      change_list[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(change_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension_->id(), change_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension_->name(), change_specifics.custom_theme_name());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(theme_extension_).spec(),
            change_specifics.custom_theme_update_url());
}

TEST_F(ThemeSyncableServiceTest, StopSync) {
  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Start syncing.
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(error.IsSet()) << error.message();
  EXPECT_EQ(0u, change_list.size());

  // Stop syncing.
  theme_sync_service_->StopSyncing(syncer::THEMES);

  // Change current theme to custom theme and notify theme_sync_service_.
  // No change is output because sync has stopped.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(0u, change_list.size());

  // ProcessSyncChanges() should return error when sync has stopped.
  error = theme_sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_TRUE(error.IsSet());
  EXPECT_EQ(syncer::THEMES, error.type());
  EXPECT_EQ("Theme syncable service is not started.",
            error.message());
}

TEST_F(ThemeSyncableServiceTest, RestoreSystemThemeBitWhenChangeToCustomTheme) {
  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  // Initialize to use system theme.
  fake_theme_service_->UseDefaultTheme();
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();

  // Change to custom theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be preserved.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(1u, change_list.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      change_list[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(change_specifics.use_system_theme_by_default());
}

#if defined(TOOLKIT_GTK)
TEST_F(ThemeSyncableServiceTest,
       GtkUpdateSystemThemeBitWhenChangeBetweenSystemAndDefault) {
  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  // Initialize to use native theme.
  fake_theme_service_->SetNativeTheme();
  fake_theme_service_->MarkClean();
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_FALSE(fake_theme_service_->is_dirty());

  // Change to default theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be false.
  fake_theme_service_->UseDefaultTheme();
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(1u, change_list.size());
  EXPECT_FALSE(change_list[0].sync_data().GetSpecifics().theme()
               .use_system_theme_by_default());

  // Change to native theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be true.
  change_list.clear();
  fake_theme_service_->SetNativeTheme();
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(1u, change_list.size());
  EXPECT_TRUE(change_list[0].sync_data().GetSpecifics().theme()
              .use_system_theme_by_default());
}
#endif

#ifndef TOOLKIT_GTK
TEST_F(ThemeSyncableServiceTest,
       NonGtkPreserveSystemThemeBitWhenChangeToDefaultTheme) {
  syncer::SyncChangeList change_list;
  static_cast<FakeSyncChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(&change_list);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Initialize to use custom theme with use_system_theme_by_default set true.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_name(kCustomThemeUrl);
  theme_specifics.set_use_system_theme_by_default(true);
  syncer::SyncError error = theme_sync_service_->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      fake_change_processor_.Pass(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock())).
          error();
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());

  // Change to default theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be preserved.
  fake_theme_service_->UseDefaultTheme();
  theme_sync_service_->OnThemeChange();
  EXPECT_EQ(1u, change_list.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      change_list[0].sync_data().GetSpecifics().theme();
  EXPECT_FALSE(change_specifics.use_custom_theme());
  EXPECT_TRUE(change_specifics.use_system_theme_by_default());
}
#endif
