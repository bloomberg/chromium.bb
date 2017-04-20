// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_model.h"

#include <set>
#include <string>

#include "ash/shelf/shelf_model_observer.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// ShelfModelObserver implementation that tracks what message are invoked.
class TestShelfModelObserver : public ShelfModelObserver {
 public:
  TestShelfModelObserver() {}

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
  void ShelfItemAdded(int) override { added_count_++; }
  void ShelfItemRemoved(int, const ShelfItem&) override { removed_count_++; }
  void ShelfItemChanged(int, const ShelfItem&) override { changed_count_++; }
  void ShelfItemMoved(int, int) override { moved_count_++; }

 private:
  void AddToResult(const std::string& format, int count, std::string* result) {
    if (!count)
      return;
    if (!result->empty())
      *result += " ";
    *result += base::StringPrintf(format.c_str(), count);
  }

  int added_count_ = 0;
  int removed_count_ = 0;
  int changed_count_ = 0;
  int moved_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

}  // namespace

class ShelfModelTest : public testing::Test {
 public:
  ShelfModelTest() {}
  ~ShelfModelTest() override {}

  void SetUp() override {
    model_.reset(new ShelfModel);
    observer_.reset(new TestShelfModelObserver);
    EXPECT_EQ(0, model_->item_count());

    ShelfItem item;
    item.type = TYPE_APP_LIST;
    model_->Add(item);
    EXPECT_EQ(1, model_->item_count());

    model_->AddObserver(observer_.get());
  }

  void TearDown() override {
    observer_.reset();
    model_.reset();
  }

  std::unique_ptr<ShelfModel> model_;
  std::unique_ptr<TestShelfModelObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfModelTest);
};

TEST_F(ShelfModelTest, BasicAssertions) {
  // Add an item.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  int index = model_->Add(item);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Change to a platform app item.
  ShelfID original_id = model_->items()[index].id;
  item.type = TYPE_APP;
  model_->Set(index, item);
  EXPECT_EQ(original_id, model_->items()[index].id);
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);

  // Remove the item.
  model_->RemoveItemAt(index);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ("removed=1", observer_->StateStringAndClear());

  // Add an app item.
  item.type = TYPE_PINNED_APP;
  index = model_->Add(item);
  observer_->StateStringAndClear();

  // Change everything.
  model_->Set(index, item);
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[index].type);

  // Add another item.
  item.type = TYPE_PINNED_APP;
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

  // App items should be after the browser shortcut.
  ShelfItem item;
  item.type = TYPE_APP;
  int platform_app_index1 = model_->Add(item);
  EXPECT_EQ(2, platform_app_index1);

  // Add another platform app item, it should follow first.
  int platform_app_index2 = model_->Add(item);
  EXPECT_EQ(3, platform_app_index2);

  // TYPE_PINNED_APP priority is higher than TYPE_APP but same as
  // TYPE_BROWSER_SHORTCUT. So TYPE_PINNED_APP is located after
  // TYPE_BROWSER_SHORTCUT.
  item.type = TYPE_PINNED_APP;
  int app_shortcut_index1 = model_->Add(item);
  EXPECT_EQ(2, app_shortcut_index1);

  item.type = TYPE_PINNED_APP;
  int app_shortcut_index2 = model_->Add(item);
  EXPECT_EQ(3, app_shortcut_index2);

  // Check that AddAt() figures out the correct indexes for app shortcuts.
  // TYPE_PINNED_APP and TYPE_BROWSER_SHORTCUT has the same weight.
  // So TYPE_PINNED_APP is located at index 0. And, TYPE_BROWSER_SHORTCUT is
  // located at index 1.
  item.type = TYPE_PINNED_APP;
  int app_shortcut_index3 = model_->AddAt(1, item);
  EXPECT_EQ(1, app_shortcut_index3);

  item.type = TYPE_PINNED_APP;
  int app_shortcut_index4 = model_->AddAt(6, item);
  EXPECT_EQ(5, app_shortcut_index4);

  item.type = TYPE_PINNED_APP;
  int app_shortcut_index5 = model_->AddAt(3, item);
  EXPECT_EQ(3, app_shortcut_index5);

  // Before there are any panels, no icons should be right aligned.
  EXPECT_EQ(model_->item_count(), model_->FirstPanelIndex());

  // Check that AddAt() figures out the correct indexes for apps and panels.
  item.type = TYPE_APP;
  int platform_app_index3 = model_->AddAt(3, item);
  EXPECT_EQ(7, platform_app_index3);

  item.type = TYPE_APP_PANEL;
  int app_panel_index1 = model_->AddAt(2, item);
  EXPECT_EQ(10, app_panel_index1);

  item.type = TYPE_APP;
  int platform_app_index4 = model_->AddAt(11, item);
  EXPECT_EQ(10, platform_app_index4);

  item.type = TYPE_APP_PANEL;
  int app_panel_index2 = model_->AddAt(12, item);
  EXPECT_EQ(12, app_panel_index2);

  item.type = TYPE_APP;
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

// Test that the indexes for the running applications are properly determined.
TEST_F(ShelfModelTest, FirstRunningAppIndex) {
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
  item.type = TYPE_PINNED_APP;
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(3, model_->FirstRunningAppIndex());

  // Insert a two app items and check the first running app index.
  item.type = TYPE_APP;
  EXPECT_EQ(3, model_->Add(item));
  EXPECT_EQ(3, model_->FirstRunningAppIndex());
  EXPECT_EQ(4, model_->Add(item));
  EXPECT_EQ(3, model_->FirstRunningAppIndex());
}

// Assertions around id generation and usage.
TEST_F(ShelfModelTest, ShelfIDTests) {
  // Get the next to use ID counter.
  ShelfID id = model_->next_id();

  // Calling this function multiple times does not change the returned ID.
  EXPECT_EQ(model_->next_id(), id);

  // Adding another item to the list should produce a new ID.
  ShelfItem item;
  item.type = TYPE_APP;
  model_->Add(item);
  EXPECT_NE(model_->next_id(), id);
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
  item.type = TYPE_PINNED_APP;
  int app1_index = model_->Add(item);
  EXPECT_EQ(2, app1_index);
  int app2_index = model_->Add(item);
  EXPECT_EQ(3, app2_index);
  int app3_index = model_->Add(item);
  EXPECT_EQ(4, app3_index);

  // Now change the type of the second item and make sure that it is moving
  // behind the shortcuts.
  item.type = TYPE_APP;
  model_->Set(app2_index, item);

  // The item should have moved in front of the app launcher.
  EXPECT_EQ(TYPE_APP, model_->items()[4].type);
}

// Test conversion between ShelfID and application [launch] ids.
TEST_F(ShelfModelTest, IdentifierConversion) {
  const std::string app_id1("app_id1");
  const std::string launch_id("launch_id");
  const ShelfID unknown_shelf_id = 123;

  // Expect kInvalidShelfID and empty app ids for input not found in the model.
  EXPECT_EQ(kInvalidShelfID, model_->GetShelfIDForAppID(std::string()));
  EXPECT_EQ(kInvalidShelfID, model_->GetShelfIDForAppID(app_id1));
  EXPECT_EQ(kInvalidShelfID,
            model_->GetShelfIDForAppIDAndLaunchID(app_id1, std::string()));
  EXPECT_EQ(kInvalidShelfID,
            model_->GetShelfIDForAppIDAndLaunchID(app_id1, launch_id));
  EXPECT_TRUE(model_->GetAppIDForShelfID(kInvalidShelfID).empty());
  EXPECT_TRUE(model_->GetAppIDForShelfID(unknown_shelf_id).empty());

  // Add an example app with an app id and a launch id.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  item.app_launch_id = AppLaunchId(app_id1, launch_id);
  const ShelfID assigned_shelf_id1 = model_->next_id();
  const int index = model_->Add(item);

  // Ensure the item ids can be found and converted as expected.
  EXPECT_NE(kInvalidShelfID, assigned_shelf_id1);
  EXPECT_EQ(assigned_shelf_id1, model_->GetShelfIDForAppID(app_id1));
  EXPECT_EQ(assigned_shelf_id1,
            model_->GetShelfIDForAppIDAndLaunchID(app_id1, launch_id));
  EXPECT_EQ(app_id1, model_->GetAppIDForShelfID(assigned_shelf_id1));

  // Removing the example app should again yield invalid ids.
  model_->RemoveItemAt(index);
  EXPECT_EQ(kInvalidShelfID, model_->GetShelfIDForAppID(app_id1));
  EXPECT_EQ(kInvalidShelfID,
            model_->GetShelfIDForAppIDAndLaunchID(app_id1, launch_id));
  EXPECT_TRUE(model_->GetAppIDForShelfID(assigned_shelf_id1).empty());

  // Add an example app with a different app id and no launch id.
  const std::string app_id2("app_id2");
  item.app_launch_id = AppLaunchId(app_id2);
  const ShelfID assigned_shelf_id2 = model_->next_id();
  model_->Add(item);

  // Ensure the item ids can be found and converted as expected.
  EXPECT_NE(kInvalidShelfID, assigned_shelf_id2);
  EXPECT_NE(assigned_shelf_id1, assigned_shelf_id2);
  EXPECT_EQ(assigned_shelf_id2, model_->GetShelfIDForAppID(app_id2));
  EXPECT_EQ(assigned_shelf_id2,
            model_->GetShelfIDForAppIDAndLaunchID(app_id2, std::string()));
  EXPECT_EQ(kInvalidShelfID,
            model_->GetShelfIDForAppIDAndLaunchID(app_id2, launch_id));
  EXPECT_EQ(app_id2, model_->GetAppIDForShelfID(assigned_shelf_id2));
}

// Test pinning and unpinning a closed app, and checking if it is pinned.
TEST_F(ShelfModelTest, ClosedAppPinning) {
  const std::string app_id("app_id");

  // Check the initial state.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());

  // Pinning a previously unknown app should add an item.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(app_id, model_->items()[1].app_launch_id.app_id());

  // Pinning the same app id again should have no change.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(app_id, model_->items()[1].app_launch_id.app_id());

  // Unpinning the app should remove the item.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());

  // Unpinning the same app id again should have no change.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
}

// Test pinning and unpinning a running app, and checking if it is pinned.
TEST_F(ShelfModelTest, RunningAppPinning) {
  const std::string app_id("app_id");

  // Check the initial state.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());

  // Add an example running app.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.app_launch_id = AppLaunchId(app_id);
  const ShelfID assigned_shelf_id = model_->next_id();
  const int index = model_->Add(item);

  // The item should be added but not pinned.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(app_id, model_->items()[index].app_launch_id.app_id());
  EXPECT_EQ(assigned_shelf_id, model_->items()[index].id);

  // Pinning the item should just change its type.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[index].type);
  EXPECT_EQ(app_id, model_->items()[index].app_launch_id.app_id());
  EXPECT_EQ(assigned_shelf_id, model_->items()[index].id);

  // Pinning the same app id again should have no change.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[index].type);
  EXPECT_EQ(app_id, model_->items()[index].app_launch_id.app_id());
  EXPECT_EQ(assigned_shelf_id, model_->items()[index].id);

  // Unpinning the app should leave the item unpinnned but running.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(app_id, model_->items()[index].app_launch_id.app_id());
  EXPECT_EQ(assigned_shelf_id, model_->items()[index].id);

  // Unpinning the same app id again should have no change.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(app_id, model_->items()[index].app_launch_id.app_id());
  EXPECT_EQ(assigned_shelf_id, model_->items()[index].id);
}

}  // namespace ash
