// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"


// Helpers --------------------------------------------------------------------

namespace history {

namespace {

AutocompleteMatch::Type ConvertedMatchType(AutocompleteMatch::Type type) {
  return ShortcutsBackend::Shortcut(
      std::string(), string16(), ShortcutsBackend::Shortcut::MatchCore(
          AutocompleteMatch(NULL, 0, 0, type)),
      base::Time::Now(), 0).match_core.type;
}

ShortcutsBackend::Shortcut::MatchCore MatchCoreForTesting(
    const std::string& url) {
  AutocompleteMatch match;
  match.destination_url = GURL(url);
  match.contents = ASCIIToUTF16("test");
  return ShortcutsBackend::Shortcut::MatchCore(match);
}

}  // namespace


// ShortcutsBackendTest -------------------------------------------------------

class ShortcutsBackendTest : public testing::Test,
                             public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsBackendTest()
      : ui_thread_(content::BrowserThread::UI, &ui_message_loop_),
        db_thread_(content::BrowserThread::DB),
        load_notified_(false),
        changed_notified_(false) {}

  virtual void SetUp();
  virtual void TearDown();

  virtual void OnShortcutsLoaded() OVERRIDE;
  virtual void OnShortcutsChanged() OVERRIDE;

  void InitBackend();

  TestingProfile profile_;
  scoped_refptr<ShortcutsBackend> backend_;
  base::MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  bool load_notified_;
  bool changed_notified_;
};

void ShortcutsBackendTest::SetUp() {
  db_thread_.Start();
  ShortcutsBackendFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &ShortcutsBackendFactory::BuildProfileForTesting);
  backend_ = ShortcutsBackendFactory::GetForProfile(&profile_);
  ASSERT_TRUE(backend_.get());
  backend_->AddObserver(this);
}

void ShortcutsBackendTest::TearDown() {
  backend_->RemoveObserver(this);
  db_thread_.Stop();
}

void ShortcutsBackendTest::OnShortcutsLoaded() {
  load_notified_ = true;
  base::MessageLoop::current()->Quit();
}

void ShortcutsBackendTest::OnShortcutsChanged() {
  changed_notified_ = true;
}

void ShortcutsBackendTest::InitBackend() {
  ShortcutsBackend* backend =
      ShortcutsBackendFactory::GetForProfile(&profile_).get();
  ASSERT_TRUE(backend);
  ASSERT_FALSE(load_notified_);
  ASSERT_FALSE(backend_->initialized());
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(load_notified_);
  EXPECT_TRUE(backend_->initialized());
}


// Actual tests ---------------------------------------------------------------

// Verifies that particular original match types are automatically modified when
// creating shortcuts.
TEST_F(ShortcutsBackendTest, ChangeMatchTypeOnShortcutCreation) {
  struct {
    AutocompleteMatch::Type input_type;
    AutocompleteMatch::Type output_type;
  } type_cases[] = {
    { AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      AutocompleteMatchType::HISTORY_URL },
    { AutocompleteMatchType::NAVSUGGEST,
      AutocompleteMatchType::HISTORY_URL },
    { AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      AutocompleteMatchType::SEARCH_HISTORY },
    { AutocompleteMatchType::SEARCH_SUGGEST,
      AutocompleteMatchType::SEARCH_HISTORY },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(type_cases); ++i) {
    EXPECT_EQ(type_cases[i].output_type,
              ConvertedMatchType(type_cases[i].input_type));
  }
}

TEST_F(ShortcutsBackendTest, AddAndUpdateShortcut) {
  InitBackend();
  EXPECT_FALSE(changed_notified_);
  ShortcutsBackend::Shortcut shortcut(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", ASCIIToUTF16("goog"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut));
  EXPECT_TRUE(changed_notified_);
  changed_notified_ = false;

  const ShortcutsBackend::ShortcutMap& shortcuts = backend_->shortcuts_map();
  ASSERT_TRUE(shortcuts.end() != shortcuts.find(shortcut.text));
  EXPECT_EQ(shortcut.id, shortcuts.find(shortcut.text)->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcuts.find(shortcut.text)->second.match_core.contents);
  shortcut.match_core.contents = ASCIIToUTF16("Google Web Search");
  EXPECT_TRUE(backend_->UpdateShortcut(shortcut));
  EXPECT_TRUE(changed_notified_);
  EXPECT_EQ(shortcut.id, shortcuts.find(shortcut.text)->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcuts.find(shortcut.text)->second.match_core.contents);
}

TEST_F(ShortcutsBackendTest, DeleteShortcuts) {
  InitBackend();
  ShortcutsBackend::Shortcut shortcut1(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", ASCIIToUTF16("goog"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut1));

  ShortcutsBackend::Shortcut shortcut2(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", ASCIIToUTF16("gle"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut2));

  ShortcutsBackend::Shortcut shortcut3(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", ASCIIToUTF16("sp"),
      MatchCoreForTesting("http://www.sport.com"), base::Time::Now(), 10);
  EXPECT_TRUE(backend_->AddShortcut(shortcut3));

  ShortcutsBackend::Shortcut shortcut4(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E2", ASCIIToUTF16("mov"),
      MatchCoreForTesting("http://www.film.com"), base::Time::Now(), 10);
  EXPECT_TRUE(backend_->AddShortcut(shortcut4));

  const ShortcutsBackend::ShortcutMap& shortcuts = backend_->shortcuts_map();

  ASSERT_EQ(4U, shortcuts.size());
  EXPECT_EQ(shortcut1.id, shortcuts.find(shortcut1.text)->second.id);
  EXPECT_EQ(shortcut2.id, shortcuts.find(shortcut2.text)->second.id);
  EXPECT_EQ(shortcut3.id, shortcuts.find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts.find(shortcut4.text)->second.id);

  EXPECT_TRUE(backend_->DeleteShortcutsWithUrl(
      shortcut1.match_core.destination_url));

  ASSERT_EQ(2U, shortcuts.size());
  EXPECT_TRUE(shortcuts.end() == shortcuts.find(shortcut1.text));
  EXPECT_TRUE(shortcuts.end() == shortcuts.find(shortcut2.text));
  ASSERT_TRUE(shortcuts.end() != shortcuts.find(shortcut3.text));
  ASSERT_TRUE(shortcuts.end() != shortcuts.find(shortcut4.text));
  EXPECT_EQ(shortcut3.id, shortcuts.find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts.find(shortcut4.text)->second.id);

  std::vector<std::string> deleted_ids;
  deleted_ids.push_back(shortcut3.id);
  deleted_ids.push_back(shortcut4.id);

  EXPECT_TRUE(backend_->DeleteShortcutsWithIds(deleted_ids));

  ASSERT_EQ(0U, shortcuts.size());
}

}  // namespace history
