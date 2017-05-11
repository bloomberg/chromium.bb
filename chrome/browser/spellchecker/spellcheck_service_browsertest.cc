// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace {

// A corrupted BDICT data used in DeleteCorruptedBDICT. Please do not use this
// BDICT data for other tests.
const uint8_t kCorruptedBDICT[] = {
    0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x3b, 0x00, 0x00, 0x00, 0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
    0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35, 0x32, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
    0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6,
    0x49, 0x00, 0x68, 0x02, 0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
};

}  // namespace

class SpellcheckServiceBrowserTest : public InProcessBrowserTest,
                                     public spellcheck::mojom::SpellChecker {
 public:
  SpellcheckServiceBrowserTest() : binding_(this) {}

  void SetUpOnMainThread() override {
    renderer_.reset(new content::MockRenderProcessHost(GetContext()));
    prefs_ = user_prefs::UserPrefs::Get(GetContext());
  }

  void TearDownOnMainThread() override {
    binding_.Close();
    prefs_ = nullptr;
    renderer_.reset();
  }

  BrowserContext* GetContext() {
    return static_cast<BrowserContext*>(browser()->profile());
  }

  PrefService* GetPrefs() {
    return prefs_;
  }

  void InitSpellcheck(bool enable_spellcheck,
                      const std::string& single_dictionary,
                      const std::string& multiple_dictionaries) {
    prefs_->SetBoolean(spellcheck::prefs::kEnableSpellcheck,
                       enable_spellcheck);
    prefs_->SetString(spellcheck::prefs::kSpellCheckDictionary,
                      single_dictionary);
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries_value);

    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForRenderProcessId(renderer_->GetID());
    ASSERT_NE(nullptr, spellcheck);

    // Override |renderer_| requests for the spellcheck::mojom::SpellChecker
    // interface so we can test the SpellChecker request flow.
    renderer_->OverrideBinderForTesting(
        spellcheck::mojom::SpellChecker::Name_,
        base::Bind(&SpellcheckServiceBrowserTest::Bind,
                   base::Unretained(this)));
  }

  void EnableSpellcheck(bool enable_spellcheck) {
    prefs_->SetBoolean(spellcheck::prefs::kEnableSpellcheck,
                       enable_spellcheck);
  }

  void ChangeCustomDictionary() {
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForRenderProcessId(renderer_->GetID());
    ASSERT_NE(nullptr, spellcheck);

    SpellcheckCustomDictionary::Change change;
    change.RemoveWord("1");
    change.AddWord("2");
    change.AddWord("3");

    spellcheck->OnCustomDictionaryChanged(change);
  }

  void SetSingleLanguageDictionary(const std::string& single_dictionary) {
    prefs_->SetString(spellcheck::prefs::kSpellCheckDictionary,
                      single_dictionary);
  }

  void SetMultiLingualDictionaries(const std::string& multiple_dictionaries) {
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries_value);
  }

  std::string GetMultilingualDictionaries() {
    const base::ListValue* list_value =
        prefs_->GetList(spellcheck::prefs::kSpellCheckDictionaries);
    std::vector<base::StringPiece> dictionaries;
    for (const auto& item_value : *list_value) {
      base::StringPiece dictionary;
      EXPECT_TRUE(item_value.GetAsString(&dictionary));
      dictionaries.push_back(dictionary);
    }
    return base::JoinString(dictionaries, ",");
  }

  void SetAcceptLanguages(const std::string& accept_languages) {
    prefs_->SetString(prefs::kAcceptLanguages, accept_languages);
  }

  bool GetEnableSpellcheckState(bool initial_state = false) {
    spellcheck_enabled_state_ = initial_state;
    RunTestRunLoop();
    EXPECT_TRUE(initialize_spellcheck_called_);
    EXPECT_TRUE(bound_connection_closed_);
    return spellcheck_enabled_state_;
  }

  bool GetCustomDictionaryChangedState() {
    RunTestRunLoop();
    EXPECT_TRUE(bound_connection_closed_);
    return custom_dictionary_changed_called_;
  }

 private:
  // Spins a RunLoop to deliver the Mojo SpellChecker request flow.
  void RunTestRunLoop() {
    bound_connection_closed_ = false;
    initialize_spellcheck_called_ = false;
    custom_dictionary_changed_called_ = false;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Binds requests for the SpellChecker interface.
  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler(
        base::Bind(&SpellcheckServiceBrowserTest::BoundConnectionClosed,
                   base::Unretained(this)));
  }

  // The requester closes (disconnects) when done.
  void BoundConnectionClosed() {
    bound_connection_closed_ = true;
    binding_.Close();
    if (quit_)
      std::move(quit_).Run();
  }

  // spellcheck::mojom::SpellChecker:
  void Initialize(
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
      const std::vector<std::string>& custom_words,
      bool enable) override {
    initialize_spellcheck_called_ = true;
    spellcheck_enabled_state_ = enable;
  }

  void CustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override {
    custom_dictionary_changed_called_ = true;
    EXPECT_EQ(1u, words_removed.size());
    EXPECT_EQ(2u, words_added.size());
  }

  // Mocked RenderProcessHost.
  std::unique_ptr<content::MockRenderProcessHost> renderer_;

  // Not owned preferences service.
  PrefService* prefs_;

  // Binding to receive the SpellChecker request flow.
  mojo::Binding<spellcheck::mojom::SpellChecker> binding_;

  // Quits the RunLoop on SpellChecker request flow completion.
  base::OnceClosure quit_;

  // Used to verify the SpellChecker request flow.
  bool bound_connection_closed_;
  bool custom_dictionary_changed_called_;
  bool initialize_spellcheck_called_;
  bool spellcheck_enabled_state_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceBrowserTest);
};

// Removing a spellcheck language from accept languages should remove it from
// spellcheck languages list as well.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       RemoveSpellcheckLanguageFromAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,es,ru");
  EXPECT_EQ("en-US", GetMultilingualDictionaries());
}

// Keeping spellcheck languages in accept languages should not alter spellcheck
// languages list.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       KeepSpellcheckLanguagesInAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,fr,es");
  EXPECT_EQ("en-US,fr", GetMultilingualDictionaries());
}

// Starting with spellcheck enabled should send the 'enable spellcheck' message
// to the renderer. Consequently disabling spellcheck should send the 'disable
// spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithSpellcheck) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with only a single-language spellcheck setting should send the
// 'enable spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithSingularLanguagePreference) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with a multi-language spellcheck setting should send the 'enable
// spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithMultiLanguagePreference) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with both single-language and multi-language spellcheck settings
// should send the 'enable spellcheck' message to the renderer. Consequently
// removing spellcheck languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithBothLanguagePreferences) {
  InitSpellcheck(true, "en-US", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting without spellcheck languages should send the 'disable spellcheck'
// message to the renderer. Consequently adding spellchecking languages should
// enable spellcheck.
// Flaky, see https://crbug.com/600153
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       DISABLED_StartWithoutLanguages) {
  InitSpellcheck(true, "", "");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  SetMultiLingualDictionaries("en-US");
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// Starting with spellcheck disabled should send the 'disable spellcheck'
// message to the renderer. Consequently enabling spellcheck should send the
// 'enable spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithoutSpellcheck) {
  InitSpellcheck(false, "", "en-US,fr");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  EnableSpellcheck(true);
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// A custom dictionary state change should send a 'custom dictionary changed'
// message to the renderer, regardless of the spellcheck enabled state.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, CustomDictionaryChanged) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());
}

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT) {
  // Write the corrupted BDICT data to create a corrupted BDICT file.
  base::FilePath dict_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  base::FilePath bdict_path =
      spellcheck::GetVersionedFileName("en-US", dict_dir);

  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    size_t actual = base::WriteFile(
        bdict_path, reinterpret_cast<const char*>(kCorruptedBDICT),
        arraysize(kCorruptedBDICT));
    EXPECT_EQ(arraysize(kCorruptedBDICT), actual);
  }

  // Attach an event to the SpellcheckService object so we can receive its
  // status updates.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  SpellcheckService::AttachStatusEvent(&event);

  BrowserContext* context = GetContext();

  // Ensure that the SpellcheckService object does not already exist. Otherwise
  // the next line will not force creation of the SpellcheckService and the
  // test will fail.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context,
          false));
  ASSERT_EQ(NULL, service);

  // Getting the spellcheck_service will initialize the SpellcheckService
  // object with the corrupted BDICT file created above since the hunspell
  // dictionary is loaded in the SpellcheckService constructor right now.
  // The SpellCheckHost object will send a BDICT_CORRUPTED event.
  SpellcheckServiceFactory::GetForContext(context);

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(SpellcheckService::BDICT_CORRUPTED,
            SpellcheckService::GetStatusEvent());
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (base::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(base::DeleteFile(bdict_path, true));
  }
}

// Checks that preferences migrate correctly.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesMigrated) {
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                  base::ListValue());
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "en-US");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that preferences are not migrated when they shouldn't be.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "fr");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have not been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that, if a user has spellchecking disabled, nothing changes
// during migration.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SpellcheckingDisabledPreferenceMigration) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(spellcheck::prefs::kEnableSpellcheck, false);

  // Migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(spellcheck::prefs::kEnableSpellcheck));
  EXPECT_EQ(1U, GetPrefs()
                    ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                    ->GetSize());
}

// Make sure preferences get preserved and spellchecking stays enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualPreferenceNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  dictionaries.AppendString("fr");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(spellcheck::prefs::kEnableSpellcheck, true);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(
      GetPrefs()->GetBoolean(spellcheck::prefs::kEnableSpellcheck));
  EXPECT_EQ(2U, GetPrefs()
                    ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                    ->GetSize());
  std::string pref;
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &pref));
  EXPECT_EQ("en-US", pref);
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(1, &pref));
  EXPECT_EQ("fr", pref);
}
