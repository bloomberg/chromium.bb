// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_provider_shortcut.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/common/guid.h"
#include "content/test/test_browser_thread.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using shortcuts_provider::Shortcut;
using shortcuts_provider::ShortcutMap;

namespace history {

const base::TimeDelta kMaxRequestWaitTimeout = base::TimeDelta::FromSeconds(1);

class ShortcutsBackendTest : public testing::Test,
                             public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsBackendTest()
      : ui_thread_(BrowserThread::UI, &ui_message_loop_),
        db_thread_(BrowserThread::DB),
        load_notified_(false) {}

  void SetUp();
  void TearDown();

  virtual void OnShortcutsLoaded() OVERRIDE;
  virtual void OnShortcutsChanged() OVERRIDE;

  void InitBackend();

  ScopedTempDir temp_dir_;
  scoped_refptr<ShortcutsBackend> backend_;
  MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  bool load_notified_;
};

void ShortcutsBackendTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  backend_ = new ShortcutsBackend(temp_dir_.path(), NULL);
  backend_->AddObserver(this);
  db_thread_.Start();
}

void ShortcutsBackendTest::TearDown() {
  backend_->RemoveObserver(this);
  db_thread_.Stop();
}

void ShortcutsBackendTest::OnShortcutsLoaded() {
  load_notified_ = true;
  MessageLoop::current()->Quit();
}

void ShortcutsBackendTest::OnShortcutsChanged() {
}

void ShortcutsBackendTest::InitBackend() {
  EXPECT_FALSE(load_notified_);
  EXPECT_FALSE(backend_->initialized());
  EXPECT_TRUE(backend_->Init());
  MessageLoop::current()->Run();
  EXPECT_TRUE(load_notified_);
  EXPECT_TRUE(backend_->initialized());
}

TEST_F(ShortcutsBackendTest, AddAndUpdateShortcut) {
  InitBackend();
  Shortcut shortcut("BD85DBA2-8C29-49F9-84AE-48E1E90880DF",
                    ASCIIToUTF16("goog"), ASCIIToUTF16("http://www.google.com"),
                    ASCIIToUTF16("Google"), ASCIIToUTF16("0,1"),
                    ASCIIToUTF16("Google"), ASCIIToUTF16("0,1"),
                    base::Time::Now().ToInternalValue(),
                    100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut));

  const ShortcutMap& shortcuts = backend_->shortcuts_map();
  ASSERT_TRUE(shortcuts.end() != shortcuts.find(shortcut.text));
  EXPECT_EQ(shortcut.id, shortcuts.find(shortcut.text)->second.id);
  EXPECT_EQ(shortcut.contents, shortcuts.find(shortcut.text)->second.contents);
  shortcut.contents = ASCIIToUTF16("Google Web Search");
  EXPECT_TRUE(backend_->UpdateShortcut(shortcut));
  EXPECT_EQ(shortcut.id, shortcuts.find(shortcut.text)->second.id);
  EXPECT_EQ(shortcut.contents, shortcuts.find(shortcut.text)->second.contents);
}

TEST_F(ShortcutsBackendTest, DeleteShortcuts) {
  InitBackend();
  Shortcut shortcut1("BD85DBA2-8C29-49F9-84AE-48E1E90880DF",
                     ASCIIToUTF16("goog"),
                     ASCIIToUTF16("http://www.google.com"),
                     ASCIIToUTF16("Google"), ASCIIToUTF16("0,1,4,0"),
                     ASCIIToUTF16("Google"), ASCIIToUTF16("0,3,4,1"),
                     base::Time::Now().ToInternalValue(),
                     100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut1));

  Shortcut shortcut2("BD85DBA2-8C29-49F9-84AE-48E1E90880E0",
                     ASCIIToUTF16("gle"), ASCIIToUTF16("http://www.google.com"),
                     ASCIIToUTF16("Google"), ASCIIToUTF16("0,1"),
                     ASCIIToUTF16("Google"), ASCIIToUTF16("0,1"),
                     base::Time::Now().ToInternalValue(),
                     100);
  EXPECT_TRUE(backend_->AddShortcut(shortcut2));

  Shortcut shortcut3("BD85DBA2-8C29-49F9-84AE-48E1E90880E1",
                     ASCIIToUTF16("sp"), ASCIIToUTF16("http://www.sport.com"),
                     ASCIIToUTF16("Sports"), ASCIIToUTF16("0,1"),
                     ASCIIToUTF16("Sport news"), ASCIIToUTF16("0,1"),
                     base::Time::Now().ToInternalValue(),
                     10);
  EXPECT_TRUE(backend_->AddShortcut(shortcut3));

  Shortcut shortcut4("BD85DBA2-8C29-49F9-84AE-48E1E90880E2",
                     ASCIIToUTF16("mov"), ASCIIToUTF16("http://www.film.com"),
                     ASCIIToUTF16("Movies"), ASCIIToUTF16("0,1"),
                     ASCIIToUTF16("Movie news"), ASCIIToUTF16("0,1"),
                     base::Time::Now().ToInternalValue(),
                     10);
  EXPECT_TRUE(backend_->AddShortcut(shortcut4));

  const ShortcutMap& shortcuts = backend_->shortcuts_map();

  ASSERT_EQ(4U, shortcuts.size());
  EXPECT_EQ(shortcut1.id, shortcuts.find(shortcut1.text)->second.id);
  EXPECT_EQ(shortcut2.id, shortcuts.find(shortcut2.text)->second.id);
  EXPECT_EQ(shortcut3.id, shortcuts.find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts.find(shortcut4.text)->second.id);

  EXPECT_TRUE(backend_->DeleteShortcutsWithUrl(shortcut1.url));

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
