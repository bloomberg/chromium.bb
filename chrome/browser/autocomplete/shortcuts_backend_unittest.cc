// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_backend.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"


// ShortcutsBackendTest -------------------------------------------------------

class ShortcutsBackendTest : public testing::Test,
                             public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsBackendTest();

  history::ShortcutsDatabase::Shortcut::MatchCore MatchCoreForTesting(
    const std::string& url,
    const std::string& contents_class = std::string(),
    const std::string& description_class = std::string(),
    AutocompleteMatch::Type type = AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  virtual void SetUp();
  virtual void TearDown();

  virtual void OnShortcutsLoaded() OVERRIDE;
  virtual void OnShortcutsChanged() OVERRIDE;

  const ShortcutsBackend::ShortcutMap& shortcuts_map() const {
    return backend_->shortcuts_map();
  }
  bool changed_notified() const { return changed_notified_; }
  void set_changed_notified(bool changed_notified) {
      changed_notified_ = changed_notified;
  }

  void InitBackend();
  bool AddShortcut(const history::ShortcutsDatabase::Shortcut& shortcut);
  bool UpdateShortcut(const history::ShortcutsDatabase::Shortcut& shortcut);
  bool DeleteShortcutsWithURL(const GURL& url);
  bool DeleteShortcutsWithIDs(
      const history::ShortcutsDatabase::ShortcutIDs& deleted_ids);


 private:
  TestingProfile profile_;
  scoped_refptr<ShortcutsBackend> backend_;
  base::MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  bool load_notified_;
  bool changed_notified_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackendTest);
};

ShortcutsBackendTest::ShortcutsBackendTest()
    : ui_thread_(content::BrowserThread::UI, &ui_message_loop_),
      db_thread_(content::BrowserThread::DB),
      load_notified_(false),
      changed_notified_(false) {
}

history::ShortcutsDatabase::Shortcut::MatchCore
    ShortcutsBackendTest::MatchCoreForTesting(
    const std::string& url,
    const std::string& contents_class,
    const std::string& description_class,
    AutocompleteMatch::Type type) {
  AutocompleteMatch match(NULL, 0, 0, type);
  match.destination_url = GURL(url);
  match.contents = base::ASCIIToUTF16("test");
  match.contents_class =
      AutocompleteMatch::ClassificationsFromString(contents_class);
  match.description_class =
      AutocompleteMatch::ClassificationsFromString(description_class);
  return ShortcutsBackend::MatchToMatchCore(match);
}

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

bool ShortcutsBackendTest::AddShortcut(
    const history::ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->AddShortcut(shortcut);
}

bool ShortcutsBackendTest::UpdateShortcut(
    const history::ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->UpdateShortcut(shortcut);
}

bool ShortcutsBackendTest::DeleteShortcutsWithURL(const GURL& url) {
  return backend_->DeleteShortcutsWithURL(url);
}

bool ShortcutsBackendTest::DeleteShortcutsWithIDs(
    const history::ShortcutsDatabase::ShortcutIDs& deleted_ids) {
  return backend_->DeleteShortcutsWithIDs(deleted_ids);
}


// Actual tests ---------------------------------------------------------------

// Verifies that creating MatchCores strips classifications and sanitizes match
// types.
TEST_F(ShortcutsBackendTest, SanitizeMatchCore) {
  struct {
    std::string input_contents_class;
    std::string input_description_class;
    AutocompleteMatch::Type input_type;
    std::string output_contents_class;
    std::string output_description_class;
    AutocompleteMatch::Type output_type;
  } cases[] = {
    { "0,1,4,0", "0,3,4,1",       AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      "0,1,4,0", "0,1",           AutocompleteMatchType::HISTORY_URL },
    { "0,3,5,1", "0,2,5,0",       AutocompleteMatchType::NAVSUGGEST,
      "0,1",     "0,0",           AutocompleteMatchType::HISTORY_URL },
    { "0,1",     "0,0,11,2,15,0", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      "0,1",     "0,0",           AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",           AutocompleteMatchType::SEARCH_SUGGEST,
      "0,1",     "0,0",           AutocompleteMatchType::SEARCH_HISTORY },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    history::ShortcutsDatabase::Shortcut::MatchCore match_core(
        MatchCoreForTesting(std::string(), cases[i].input_contents_class,
                            cases[i].input_description_class,
                            cases[i].input_type));
    EXPECT_EQ(cases[i].output_contents_class, match_core.contents_class);
    EXPECT_EQ(cases[i].output_description_class, match_core.description_class);
    EXPECT_EQ(cases[i].output_type, match_core.type);
  }
}

TEST_F(ShortcutsBackendTest, AddAndUpdateShortcut) {
  InitBackend();
  EXPECT_FALSE(changed_notified());

  history::ShortcutsDatabase::Shortcut shortcut(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", base::ASCIIToUTF16("goog"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  ShortcutsBackend::ShortcutMap::const_iterator shortcut_iter(
      shortcuts_map().find(shortcut.text));
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);

  set_changed_notified(false);
  shortcut.match_core.contents = base::ASCIIToUTF16("Google Web Search");
  EXPECT_TRUE(UpdateShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  shortcut_iter = shortcuts_map().find(shortcut.text);
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);
}

TEST_F(ShortcutsBackendTest, DeleteShortcuts) {
  InitBackend();
  history::ShortcutsDatabase::Shortcut shortcut1(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", base::ASCIIToUTF16("goog"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut1));

  history::ShortcutsDatabase::Shortcut shortcut2(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", base::ASCIIToUTF16("gle"),
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut2));

  history::ShortcutsDatabase::Shortcut shortcut3(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", base::ASCIIToUTF16("sp"),
      MatchCoreForTesting("http://www.sport.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut3));

  history::ShortcutsDatabase::Shortcut shortcut4(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E2", base::ASCIIToUTF16("mov"),
      MatchCoreForTesting("http://www.film.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut4));

  ASSERT_EQ(4U, shortcuts_map().size());
  EXPECT_EQ(shortcut1.id, shortcuts_map().find(shortcut1.text)->second.id);
  EXPECT_EQ(shortcut2.id, shortcuts_map().find(shortcut2.text)->second.id);
  EXPECT_EQ(shortcut3.id, shortcuts_map().find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts_map().find(shortcut4.text)->second.id);

  EXPECT_TRUE(DeleteShortcutsWithURL(shortcut1.match_core.destination_url));

  ASSERT_EQ(2U, shortcuts_map().size());
  EXPECT_EQ(0U, shortcuts_map().count(shortcut1.text));
  EXPECT_EQ(0U, shortcuts_map().count(shortcut2.text));
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut3_iter(
      shortcuts_map().find(shortcut3.text));
  ASSERT_TRUE(shortcut3_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut3.id, shortcut3_iter->second.id);
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut4_iter(
      shortcuts_map().find(shortcut4.text));
  ASSERT_TRUE(shortcut4_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut4.id, shortcut4_iter->second.id);

  history::ShortcutsDatabase::ShortcutIDs deleted_ids;
  deleted_ids.push_back(shortcut3.id);
  deleted_ids.push_back(shortcut4.id);
  EXPECT_TRUE(DeleteShortcutsWithIDs(deleted_ids));

  ASSERT_EQ(0U, shortcuts_map().size());
}
