// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include "ash/launcher/launcher_model_observer.h"
#include "base/stringprintf.h"
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
  virtual void LauncherItemRemoved(int index) OVERRIDE {
    removed_count_++;
  }
  virtual void LauncherItemChanged(int index) OVERRIDE {
    changed_count_++;
  }
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE {
    moved_count_++;
  }
  virtual void LauncherItemWillChange(int index) OVERRIDE {
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

  // Model is initially populated with two items.
  EXPECT_EQ(2, model.item_count());

  // Add an item.
  model.AddObserver(&observer);
  LauncherItem item;
  model.Add(0, item);
  EXPECT_EQ(3, model.item_count());
  EXPECT_EQ("added=1",
            observer.StateStringAndClear());

  // Change a tabbed image.
  model.Set(0, LauncherItem());
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_TABBED, model.items()[0].type);

  // Remove the item.
  model.RemoveItemAt(0);
  EXPECT_EQ(2, model.item_count());
  EXPECT_EQ("removed=1", observer.StateStringAndClear());

  // Add an app item.
  item.type = TYPE_APP;
  model.Add(0, item);
  observer.StateStringAndClear();

  // Change everything.
  model.Set(0, item);
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_APP, model.items()[0].type);

  // Add another item.
  item.type = TYPE_APP;
  model.Add(1, item);
  observer.StateStringAndClear();

  // Move the second item to be first.
  model.Move(1, 0);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());

  // Move the first item to the second item.
  model.Move(0, 1);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());
}

}  // namespace ash
