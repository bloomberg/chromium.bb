// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include <set>
#include <string>

#include "ash/launcher/launcher_model_observer.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// LauncherModelObserver implementation that tracks what message are invoked.
class TestLauncherModelObserver : public LauncherModelObserver {
 public:
  TestLauncherModelObserver()
      : added_count_(0),
        removed_count_(0),
        changed_count_(0),
        moved_count_(0) {
  }

  // Returns a string description of the changes that have occurred since this
  // was last invoked. Resets state to initial state.
  std::string StateStringAndClear() {
    std::string result;
    AddToResult("added=%d", added_count_, &result);
    AddToResult("removed=%d", removed_count_, &result);
    AddToResult("changed=%d", changed_count_, &result);
    AddToResult("moved=%d", moved_count_, &result);
    added_count_ = removed_count_ = changed_count_ = moved_count_ = 0;
    return result;
  }

  // LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE {
    added_count_++;
  }
  virtual void LauncherItemRemoved(int index, LauncherID id) OVERRIDE {
    removed_count_++;
  }
  virtual void LauncherItemChanged(int index,
                                   const LauncherItem& old_item) OVERRIDE {
    changed_count_++;
  }
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE {
    moved_count_++;
  }
  virtual void LauncherStatusChanged() OVERRIDE {
  }

 private:
  void AddToResult(const std::string& format, int count, std::string* result) {
    if (!count)
      return;
    if (!result->empty())
      *result += " ";
    *result += base::StringPrintf(format.c_str(), count);
  }

  int added_count_;
  int removed_count_;
  int changed_count_;
  int moved_count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherModelObserver);
};

}  // namespace

TEST(LauncherModel, BasicAssertions) {
  TestLauncherModelObserver observer;
  LauncherModel model;

  // Model is initially populated with item.
  EXPECT_EQ(1, model.item_count());

  // Add an item.
  model.AddObserver(&observer);
  LauncherItem item;
  int index = model.Add(item);
  EXPECT_EQ(2, model.item_count());
  EXPECT_EQ("added=1", observer.StateStringAndClear());

  // Verifies all the items get unique ids.
  std::set<LauncherID> ids;
  for (int i = 0; i < model.item_count(); ++i)
    ids.insert(model.items()[i].id);
  EXPECT_EQ(model.item_count(), static_cast<int>(ids.size()));

  // Change a tabbed image.
  LauncherID original_id = model.items()[index].id;
  model.Set(index, LauncherItem());
  EXPECT_EQ(original_id, model.items()[index].id);
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_TABBED, model.items()[index].type);

  // Remove the item.
  model.RemoveItemAt(index);
  EXPECT_EQ(1, model.item_count());
  EXPECT_EQ("removed=1", observer.StateStringAndClear());

  // Add an app item.
  item.type = TYPE_APP_SHORTCUT;
  index = model.Add(item);
  observer.StateStringAndClear();

  // Change everything.
  model.Set(index, item);
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_APP_SHORTCUT, model.items()[index].type);

  // Add another item.
  item.type = TYPE_APP_SHORTCUT;
  model.Add(item);
  observer.StateStringAndClear();

  // Move the third to the second.
  model.Move(2, 1);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());

  // And back.
  model.Move(1, 2);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());
}

// Assertions around where items are added.
TEST(LauncherModel, AddIndices) {
  TestLauncherModelObserver observer;
  LauncherModel model;

  // Model is initially populated with one item.
  EXPECT_EQ(1, model.item_count());

  // Insert browser short cut at index 0.
  LauncherItem browser_shortcut;
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  int browser_shortcut_index = model.Add(browser_shortcut);
  EXPECT_EQ(0, browser_shortcut_index);

  // Tabbed items should be after browser shortcut.
  LauncherItem item;
  int tabbed_index1 = model.Add(item);
  EXPECT_EQ(1, tabbed_index1);

  // Add another tabbed item, it should follow first.
  int tabbed_index2 = model.Add(item);
  EXPECT_EQ(2, tabbed_index2);

  // APP_SHORTCUT's priority is higher than TABBED but same as
  // BROWSER_SHORTCUT. So APP_SHORTCUT is located after BROWSER_SHORCUT.
  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index1 = model.Add(item);
  EXPECT_EQ(1, app_shortcut_index1);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index2 = model.Add(item);
  EXPECT_EQ(2, app_shortcut_index2);

  // Check that AddAt() figures out the correct indexes for app shortcuts.
  // APP_SHORTCUT and BROWSER_SHORTCUT has the same weight.
  // So APP_SHORTCUT is located at index 0. And, BROWSER_SHORTCUT is located at
  // index 1.
  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index3 = model.AddAt(0, item);
  EXPECT_EQ(0, app_shortcut_index3);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index4 = model.AddAt(5, item);
  EXPECT_EQ(4, app_shortcut_index4);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index5 = model.AddAt(2, item);
  EXPECT_EQ(2, app_shortcut_index5);

  // Before there are any panels, no icons should be right aligned.
  EXPECT_EQ(model.item_count(), model.FirstPanelIndex());

  // Check that AddAt() figures out the correct indexes for tabs and panels.
  item.type = TYPE_TABBED;
  int tabbed_index3 = model.AddAt(2, item);
  EXPECT_EQ(6, tabbed_index3);

  item.type = TYPE_APP_PANEL;
  int app_panel_index1 = model.AddAt(2, item);
  EXPECT_EQ(10, app_panel_index1);

  item.type = TYPE_TABBED;
  int tabbed_index4 = model.AddAt(11, item);
  EXPECT_EQ(9, tabbed_index4);

  item.type = TYPE_APP_PANEL;
  int app_panel_index2 = model.AddAt(12, item);
  EXPECT_EQ(12, app_panel_index2);

  item.type = TYPE_TABBED;
  int tabbed_index5 = model.AddAt(7, item);
  EXPECT_EQ(7, tabbed_index5);

  item.type = TYPE_APP_PANEL;
  int app_panel_index3 = model.AddAt(13, item);
  EXPECT_EQ(13, app_panel_index3);

  // Right aligned index should be the first app panel index.
  EXPECT_EQ(12, model.FirstPanelIndex());

  EXPECT_EQ(TYPE_BROWSER_SHORTCUT, model.items()[1].type);
  EXPECT_EQ(TYPE_APP_LIST, model.items()[model.FirstPanelIndex() - 1].type);
}

// Assertions around id generation and usage.
TEST(LauncherModel, LauncherIDTests) {
  TestLauncherModelObserver observer;
  LauncherModel model;

  EXPECT_EQ(1, model.item_count());

  // Get the next to use ID counter.
  LauncherID id = model.next_id();

  // Calling this function multiple times does not change the returned ID.
  EXPECT_EQ(model.next_id(), id);

  // Check that when we reserve a value it will be the previously retrieved ID,
  // but it will not change the item count and retrieving the next ID should
  // produce something new.
  EXPECT_EQ(model.reserve_external_id(), id);
  EXPECT_EQ(1, model.item_count());
  LauncherID id2 = model.next_id();
  EXPECT_NE(id2, id);

  // Adding another item to the list should also produce a new ID.
  LauncherItem item;
  item.type = TYPE_TABBED;
  model.Add(item);
  EXPECT_NE(model.next_id(), id2);
}

// This verifies that converting an existing item into a lower weight category
// (e.g. shortcut to running but not pinned app) will move it to the proper
// location. See crbug.com/248769.
TEST(LauncherModel, CorrectMoveItemsWhenStateChange) {
  LauncherModel model;

  // The app list should be the last item in the list.
  EXPECT_EQ(1, model.item_count());

  // The first item is the browser.
  LauncherItem browser_shortcut;
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  int browser_shortcut_index = model.Add(browser_shortcut);
  EXPECT_EQ(0, browser_shortcut_index);

  // Add three shortcuts. They should all be moved between the two.
  LauncherItem item;
  item.type = TYPE_APP_SHORTCUT;
  int app1_index = model.Add(item);
  EXPECT_EQ(1, app1_index);
  int app2_index = model.Add(item);
  EXPECT_EQ(2, app2_index);
  int app3_index = model.Add(item);
  EXPECT_EQ(3, app3_index);

  // Now change the type of the second item and make sure that it is moving
  // behind the shortcuts.
  item.type = TYPE_PLATFORM_APP;
  model.Set(app2_index, item);

  // The item should have moved in front of the app launcher.
  EXPECT_EQ(TYPE_PLATFORM_APP, model.items()[3].type);
}

}  // namespace ash
