// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template<typename T>
void GetValueAndQuit(T* result, base::Closure quit, T actual) {
  *result = actual;
  quit.Run();
}

template<typename T>
T GetCallbackResult(base::Callback<void(base::Callback<void(T)>)> callback) {
  T result = false;
  base::RunLoop loop;
  callback.Run(base::Bind(&GetValueAndQuit<T>, &result, loop.QuitClosure()));
  loop.Run();
  return result;
}

// A pref store that can have its read_error property changed for testing.
class TestingPrefStoreWithCustomReadError : public TestingPrefStore {
 public:
  TestingPrefStoreWithCustomReadError()
      : read_error_(PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
    // By default the profile is "new" (NO_FILE means that the profile
    // wasn't found on disk, so it was created).
  }
  virtual PrefReadError GetReadError() const OVERRIDE {
    return read_error_;
  }
  virtual bool IsInitializationComplete() const OVERRIDE {
    return true;
  }
  void set_read_error(PrefReadError read_error) {
    read_error_ = read_error;
  }
 private:
  virtual ~TestingPrefStoreWithCustomReadError() {}
  PrefReadError read_error_;
};

#if defined(OS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif defined(OS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("/oo");
#endif

static scoped_refptr<extensions::Extension> CreateExtension(
    const std::string& name,
    const std::string& id) {
  DictionaryValue manifest;
  manifest.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extension_manifest_keys::kName, name);
  std::string error;
  scoped_refptr<extensions::Extension> extension =
    extensions::Extension::Create(
        base::FilePath(kExtensionFilePath).AppendASCII(name),
        extensions::Manifest::INTERNAL,
        manifest,
        extensions::Extension::NO_FLAGS,
        id,
        &error);
  return extension;
}

}  // namespace

class ProfileSigninConfirmationDialogTest : public testing::Test {
 public:
  ProfileSigninConfirmationDialogTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        model_(NULL) {
  }
  virtual ~ProfileSigninConfirmationDialogTest() {}

  virtual void SetUp() OVERRIDE {
    // Create the profile.
    TestingProfile::Builder builder;
    user_prefs_ = new TestingPrefStoreWithCustomReadError;
    TestingPrefServiceSyncable* pref_service = new TestingPrefServiceSyncable(
        new TestingPrefStore(),
        user_prefs_,
        new TestingPrefStore(),
        new PrefRegistrySyncable(),
        new PrefNotifierImpl());
    chrome::RegisterUserPrefs(pref_service->registry());
    builder.SetPrefService(make_scoped_ptr<PrefServiceSyncable>(pref_service));
    profile_ = builder.Build();

    // Initialize the services we check.
    profile_->CreateBookmarkModel(true);
    model_ = BookmarkModelFactory::GetForProfile(profile_.get());
    ui_test_utils::WaitForBookmarkModelToLoad(model_);
    profile_->CreateHistoryService(true, false);
    extensions::TestExtensionSystem* system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get()));
    CommandLine command_line(CommandLine::NO_PROGRAM);
    system->CreateExtensionService(&command_line,
                                   base::FilePath(kExtensionFilePath),
                                   false);

    // Create a dialog, but don't display it.
    dialog_ = new ProfileSigninConfirmationDialog(
        profile_.get(), "",
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
  }

  virtual void TearDown() OVERRIDE {
    delete dialog_;
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  TestingPrefStoreWithCustomReadError* user_prefs_;
  ProfileSigninConfirmationDialog* dialog_;
  BookmarkModel* model_;
};

TEST_F(ProfileSigninConfirmationDialogTest, DoNotPromptForNewProfile) {
  // Profile is new and there's no profile data.
  EXPECT_FALSE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}

TEST_F(ProfileSigninConfirmationDialogTest, PromptForNewProfile_Bookmarks) {
  ASSERT_TRUE(model_);

  // Profile is new but has bookmarks.
  model_->AddURL(model_->bookmark_bar_node(), 0, string16(ASCIIToUTF16("foo")),
                 GURL("http://foo.com"));
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}

TEST_F(ProfileSigninConfirmationDialogTest, PromptForNewProfile_Extensions) {
  ExtensionService* extensions =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  ASSERT_TRUE(extensions);

  // Profile is new but has synced extensions.

  // (The web store doesn't count.)
  scoped_refptr<extensions::Extension> webstore =
      CreateExtension("web store", extension_misc::kWebStoreAppId);
  extensions->extension_prefs()->AddGrantedPermissions(
      webstore->id(),
      make_scoped_refptr(new extensions::PermissionSet));
  extensions->AddExtension(webstore);
  EXPECT_FALSE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));

  scoped_refptr<extensions::Extension> extension = CreateExtension("foo", "");
  extensions->extension_prefs()->AddGrantedPermissions(
      extension->id(),
      make_scoped_refptr(new extensions::PermissionSet));
  extensions->AddExtension(extension);
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}

TEST_F(ProfileSigninConfirmationDialogTest, PromptForNewProfile_History) {
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_.get(),
      Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(history);

  // Profile is new but has more than $(kHistoryEntriesBeforeNewProfilePrompt)
  // history items.
  char buf[18];
  for (int i = 0; i < 10; i++) {
    base::snprintf(buf, arraysize(buf), "http://foo.com/%d", i);
    history->AddPage(
        GURL(std::string(buf)), base::Time::Now(), NULL, 1,
        GURL(), history::RedirectList(), content::PAGE_TRANSITION_LINK,
        history::SOURCE_BROWSED, false);
  }
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}

TEST_F(ProfileSigninConfirmationDialogTest, PromptForNewProfile_TypedURLs) {
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_.get(),
      Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(history);

  // Profile is new but has a typed URL.
  history->AddPage(
      GURL("http://example.com"), base::Time::Now(), NULL, 1,
      GURL(), history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}

TEST_F(ProfileSigninConfirmationDialogTest, PromptForNewProfile_Restarted) {
  // Browser has been shut down since profile was created.
  user_prefs_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NONE);
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile,
              base::Unretained(dialog_))));
}
