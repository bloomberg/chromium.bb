// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_model.h"

#include <set>
#include <string>

#include "ash/ash_switches.h"
#include "ash/shelf/shelf_model_observer.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// ShelfModelObserver implementation that tracks what message are invoked.
class TestShelfModelObserver : public ShelfModelObserver {
 public:
  TestShelfModelObserver()
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

  // ShelfModelObserver overrides:
  virtual void ShelfItemAdded(int index) OVERRIDE {
    added_count_++;
  }
  virtual void ShelfItemRemoved(int index, ShelfID id) OVERRIDE {
    removed_count_++;
  }
  virtual void ShelfItemChanged(int index,
                                const ShelfItem& old_item) OVERRIDE {
    changed_count_++;
  }
  virtual void ShelfItemMoved(int start_index, int target_index) OVERRIDE {
    moved_count_++;
  }
  virtual void ShelfStatusChanged() OVERRIDE {
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

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

}  // namespace

class ShelfModelTest : public testing::Test {
 public:
  ShelfModelTest() {}
  virtual ~ShelfModelTest() {}

  virtual void SetUp() {
    model_.reset(new ShelfModel);
    observer_.reset(new TestShelfModelObserver);
    EXPECT_EQ(0, model_->item_count());

    ShelfItem item;
    item.type = TYPE_APP_LIST;
    model_->Add(item);
    EXPECT_EQ(1, model_->item_count());

    model_->AddObserver(observer_.get());
  }

  virtual void TearDown() {
    observer_.reset();
    model_.reset();
  }

  scoped_ptr<ShelfModel> model_;
  scoped_ptr<TestShelfModelObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfModelTest);
};

TEST_F(ShelfModelTest, BasicAssertions) {
  // Add an item.
  ShelfItem item;
  item.type = TYPE_APP_SHORTCUT;
  int index = model_->Add(item);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Change to a platform app item.
  ShelfID original_id = model_->items()[index].id;
  item.type = TYPE_PLATFORM_APP;
  model_->Set(index, item);
  EXPECT_EQ(original_id, model_->items()[index].id);
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_PLATFORM_APP, model_->items()[index].type);

  // Remove the item.
  model_->RemoveItemAt(index);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ("removed=1", observer_->StateStringAndClear());

  // Add an app item.
  item.type = TYPE_APP_SHORTCUT;
  index = model_->Add(item);
  observer_->StateStringAndClear();

  // Change everything.
  model_->Set(index, item);
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_APP_SHORTCUT, model_->items()[index].type);

  // Add another item.
  item.type = TYPE_APP_SHORTCUT;
  model_->Add(item);
  observer_->StateStringAndClear();

  // Move the second to the first.
  model_->Move(1, 0);
  EXPECT_EQ("moved=1", observer_->StateStringAndClear());

  // And back.
  model_->Move(0, 1);
  EXPECT_EQ("moved=1", observer_->StateStringAndClear());

  // Verifies all the items get unique ids.
  std::set<ShelfID> ids;
  for (int i = 0; i < model_->item_count(); ++i)
    ids.insert(model_->items()[i].id);
  EXPECT_EQ(model_->item_count(), static_cast<int>(ids.size()));
}

// Assertions around where items are added.
TEST_F(ShelfModelTest, AddIndices) {
  // Insert browser short cut at index 1.
  ShelfItem browser_shortcut;
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  int browser_shortcut_index = model_->Add(browser_shortcut);
  EXPECT_EQ(1, browser_shortcut_index);

  // platform app items should be after browser shortcut.
  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  int platform_app_index1 = model_->Add(item);
  EXPECT_EQ(2, platform_app_index1);

  // Add another platform app item, it should follow first.
  int platform_app_index2 = model_->Add(item);
  EXPECT_EQ(3, platform_app_index2);

  // APP_SHORTCUT priority is higher than PLATFORM_APP but same as
  // BROWSER_SHORTCUT. So APP_SHORTCUT is located after BROWSER_SHORCUT.
  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index1 = model_->Add(item);
  EXPECT_EQ(2, app_shortcut_index1);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index2 = model_->Add(item);
  EXPECT_EQ(3, app_shortcut_index2);

  // Check that AddAt() figures out the correct indexes for app shortcuts.
  // APP_SHORTCUT and BROWSER_SHORTCUT has the same weight.
  // So APP_SHORTCUT is located at index 0. And, BROWSER_SHORTCUT is located at
  // index 1.
  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index3 = model_->AddAt(1, item);
  EXPECT_EQ(1, app_shortcut_index3);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index4 = model_->AddAt(6, item);
  EXPECT_EQ(5, app_shortcut_index4);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index5 = model_->AddAt(3, item);
  EXPECT_EQ(3, app_shortcut_index5);

  // Before there are any panels, no icons should be right aligned.
  EXPECT_EQ(model_->item_count(), model_->FirstPanelIndex());

  // Check that AddAt() figures out the correct indexes for platform apps and
  // panels.
  item.type = TYPE_PLATFORM_APP;
  int platform_app_index3 = model_->AddAt(3, item);
  EXPECT_EQ(7, platform_app_index3);

  item.type = TYPE_APP_PANEL;
  int app_panel_index1 = model_->AddAt(2, item);
  EXPECT_EQ(10, app_panel_index1);

  item.type = TYPE_PLATFORM_APP;
  int platform_app_index4 = model_->AddAt(11, item);
  EXPECT_EQ(10, platform_app_index4);

  item.type = TYPE_APP_PANEL;
  int app_panel_index2 = model_->AddAt(12, item);
  EXPECT_EQ(12, app_panel_index2);

  item.type = TYPE_PLATFORM_APP;
  int platform_app_index5 = model_->AddAt(7, item);
  EXPECT_EQ(7, platform_app_index5);

  item.type = TYPE_APP_PANEL;
  int app_panel_index3 = model_->AddAt(13, item);
  EXPECT_EQ(13, app_panel_index3);

  // Right aligned index should be the first app panel index.
  EXPECT_EQ(12, model_->FirstPanelIndex());

  EXPECT_EQ(TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(TYPE_APP_LIST, model_->items()[0].type);
}

// Test that the indexes for the running applications are properly determined
// when the first running application is a windowed application.
TEST_F(ShelfModelTest, FirstRunningAppIndexUsingWindowedAppFirst) {
  // Insert the browser shortcut at index 1 and check that the running
  // application index would be behind it.
  ShelfItem item;
  item.type = TYPE_BROWSER_SHORTCUT;
  EXPECT_EQ(1, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());

  // Insert a panel application at the end and check that the running
  // application index would be at / before the application panel.
  item.type = TYPE_APP_PANEL;
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());

  // Insert an application shortcut and make sure that the running application
  // index would be behind it.
  item.type = TYPE_APP_SHORTCUT;
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(3, model_->FirstRunningAppIndex());

  // Insert different running application shortcuts - but first a windowed
  // application - and make sure that the same index gets returned.
  item.type = TYPE_WINDOWED_APP;
  int running_app_index = model_->Add(item);
  EXPECT_EQ(3, running_app_index);
  EXPECT_EQ(running_app_index, model_->FirstRunningAppIndex());
  item.type = TYPE_PLATFORM_APP;
  EXPECT_EQ(running_app_index + 1, model_->Add(item));
  EXPECT_EQ(running_app_index, model_->FirstRunningAppIndex());
}

// Test that the indexes for the running applications are properly determined
// when the first running application is a platform application.
TEST_F(ShelfModelTest, FirstRunningAppIndexUsingPlatformAppFirst) {
  // Insert the browser shortcut at index 1 and check that the running
  // application index would be behind it.
  ShelfItem item;
  item.type = TYPE_BROWSER_SHORTCUT;
  EXPECT_EQ(1, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());

  // Insert a panel application at the end and check that the running
  // application index would be at / before the application panel.
  item.type = TYPE_APP_PANEL;
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());

  // Insert an application shortcut and make sure that the running application
  // index would be behind it.
  item.type = TYPE_APP_SHORTCUT;
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(3, model_->FirstRunningAppIndex());

  // Insert different running application shortcuts - but first a platfom
  // application - and make sure that the same index gets returned.
  item.type = TYPE_PLATFORM_APP;
  int running_app_index = model_->Add(item);
  EXPECT_EQ(3, running_app_index);
  EXPECT_EQ(running_app_index, model_->FirstRunningAppIndex());
  item.type = TYPE_WINDOWED_APP;
  EXPECT_EQ(running_app_index + 1, model_->Add(item));
  EXPECT_EQ(running_app_index, model_->FirstRunningAppIndex());
}

// Assertions around id generation and usage.
TEST_F(ShelfModelTest, ShelfIDTests) {
  // Get the next to use ID counter.
  ShelfID id = model_->next_id();

  // Calling this function multiple times does not change the returned ID.
  EXPECT_EQ(model_->next_id(), id);

  // Check that when we reserve a value it will be the previously retrieved ID,
  // but it will not change the item count and retrieving the next ID should
  // produce something new.
  EXPECT_EQ(model_->reserve_external_id(), id);
  EXPECT_EQ(1, model_->item_count());
  ShelfID id2 = model_->next_id();
  EXPECT_NE(id2, id);

  // Adding another item to the list should also produce a new ID.
  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  model_->Add(item);
  EXPECT_NE(model_->next_id(), id2);
}

// This verifies that converting an existing item into a lower weight category
// (e.g. shortcut to running but not pinned app) will move it to the proper
// location. See crbug.com/248769.
TEST_F(ShelfModelTest, CorrectMoveItemsWhenStateChange) {
  // The first item is the app list and last item is the browser.
  ShelfItem browser_shortcut;
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  int browser_shortcut_index = model_->Add(browser_shortcut);
  EXPECT_EQ(TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(1, browser_shortcut_index);

  // Add three shortcuts. They should all be moved between the two.
  ShelfItem item;
  item.type = TYPE_APP_SHORTCUT;
  int app1_index = model_->Add(item);
  EXPECT_EQ(2, app1_index);
  int app2_index = model_->Add(item);
  EXPECT_EQ(3, app2_index);
  int app3_index = model_->Add(item);
  EXPECT_EQ(4, app3_index);

  // Now change the type of the second item and make sure that it is moving
  // behind the shortcuts.
  item.type = TYPE_PLATFORM_APP;
  model_->Set(app2_index, item);

  // The item should have moved in front of the app launcher.
  EXPECT_EQ(TYPE_PLATFORM_APP, model_->items()[4].type);
}

}  // namespace ash
