// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class FakeSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  bool HasWindow() const override { return false; }
  SessionID::id_type GetSessionId() const override { return 0; }
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  bool IsApp() const override { return false; }
  bool IsTypeTabbed() const override { return false; }
  bool IsTypePopup() const override { return false; }
  bool IsTabPinned(const SyncedTabDelegate* tab) const override {
    return false;
  }
  SyncedTabDelegate* GetTabAt(int index) const override { return NULL; }
  SessionID::id_type GetTabIdAt(int index) const override { return 0; }
  bool IsSessionRestoreInProgress() const override { return false;}
  bool ShouldSync() const override { return false; }
};

class FakeSyncedTabDelegate : public SyncedTabDelegate {
 public:
  FakeSyncedTabDelegate() :
      pending_index_(-1),
      current_index_(0),
      pending_entry_(NULL),
      window_(new FakeSyncedWindowDelegate()) {}

  SessionID::id_type GetWindowId() const override { return 0; }
  SessionID::id_type GetSessionId() const override { return 0; }
  bool IsBeingDestroyed() const override { return false; }
  Profile* profile() const override { return NULL; }
  std::string GetExtensionAppId() const override { return ""; }
  int GetCurrentEntryIndex() const override { return current_index_; }
  int GetEntryCount() const override { return indexed_entries_.size(); }
  int GetPendingEntryIndex() const override { return pending_index_; }
  content::NavigationEntry* GetPendingEntry() const override {
    return pending_entry_;
  }
  content::NavigationEntry* GetEntryAtIndex(int i) const override {
    return indexed_entries_[i];
  }
  content::NavigationEntry* GetActiveEntry() const override { return NULL; }
  bool ProfileIsSupervised() const override { return false; }
  const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const override { return NULL; }
  bool IsPinned() const override { return false;}
  bool HasWebContents() const override { return false;}
  content::WebContents* GetWebContents() const override { return NULL;}
  int GetSyncId() const override { return 0; }
  void SetSyncId(int sync_id) override {}
  const SyncedWindowDelegate* GetSyncedWindowDelegate() const override {
    return window_.get();
  }

  void SetCurrentEntryIndex(int i) {
    current_index_ = i;
  }

  void SetPendingEntryIndex(int i) {
    pending_index_ = i;
  }

  void SetPendingEntry(content::NavigationEntry* entry) {
    pending_entry_ = entry;
  }

  void AppendIndexedEntry(content::NavigationEntry* entry) {
    indexed_entries_.push_back(entry);
  }

 private:
  int pending_index_;
  int current_index_;
  content::NavigationEntry* pending_entry_;
  std::vector<content::NavigationEntry*> indexed_entries_;
  const scoped_ptr<FakeSyncedWindowDelegate> window_;
};

class SyncedTabDelegateTest : public testing::Test {
 public:
  SyncedTabDelegateTest() : tab_(),
      entry_(content::NavigationEntry::Create()) {}

 protected:
  FakeSyncedTabDelegate tab_;
  const scoped_ptr<content::NavigationEntry> entry_;
};

} // namespace

TEST_F(SyncedTabDelegateTest, GetEntryCurrentIsPending) {
  tab_.SetPendingEntryIndex(1);
  tab_.SetCurrentEntryIndex(1);

  tab_.SetPendingEntry(entry_.get());

  EXPECT_EQ(entry_.get(), tab_.GetCurrentEntryMaybePending());
  EXPECT_EQ(entry_.get(), tab_.GetEntryAtIndexMaybePending(1));
}

TEST_F(SyncedTabDelegateTest, GetEntryCurrentNotPending) {
  tab_.SetPendingEntryIndex(1);
  tab_.SetCurrentEntryIndex(0);

  tab_.AppendIndexedEntry(entry_.get());

  EXPECT_EQ(entry_.get(), tab_.GetCurrentEntryMaybePending());
  EXPECT_EQ(entry_.get(), tab_.GetEntryAtIndexMaybePending(0));
}

TEST_F(SyncedTabDelegateTest, ShouldSyncNoEntries) {
  EXPECT_FALSE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncNullEntry) {
  entry_->SetURL(GURL("http://www.google.com"));
  tab_.AppendIndexedEntry(entry_.get());
  tab_.AppendIndexedEntry(NULL);

  EXPECT_FALSE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncFile) {
  entry_->SetURL(GURL("file://path"));
  tab_.AppendIndexedEntry(entry_.get());

  EXPECT_FALSE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncChrome) {
  entry_->SetURL(GURL("chrome://preferences/"));
  tab_.AppendIndexedEntry(entry_.get());

  EXPECT_FALSE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncHistory) {
  entry_->SetURL(GURL("chrome://history/"));
  tab_.AppendIndexedEntry(entry_.get());

  EXPECT_TRUE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncValid) {
  entry_->SetURL(GURL("http://www.google.com"));
  tab_.AppendIndexedEntry(entry_.get());

  EXPECT_TRUE(tab_.ShouldSync());
}

TEST_F(SyncedTabDelegateTest, ShouldSyncMultiple) {
  entry_->SetURL(GURL("file://path"));
  tab_.AppendIndexedEntry(entry_.get());

  scoped_ptr<content::NavigationEntry>
      entry2(content::NavigationEntry::Create());
  entry2->SetURL(GURL("chrome://preferences/"));
  tab_.AppendIndexedEntry(entry2.get());

  // As long as they're all invalid, expect false.
  EXPECT_FALSE(tab_.ShouldSync());

  scoped_ptr<content::NavigationEntry>
      entry3(content::NavigationEntry::Create());
  entry3->SetURL(GURL("http://www.google.com"));
  tab_.AppendIndexedEntry(entry3.get());

  // With one valid, expect true.
  EXPECT_TRUE(tab_.ShouldSync());
}

} // namespace browser_sync
