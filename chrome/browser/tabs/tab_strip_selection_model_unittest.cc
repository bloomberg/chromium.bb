// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/tabs/tab_strip_selection_model.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TabStripSelectionModelTest;

// Returns the state of the selection model as a string. The format is:
// 'active=X anchor=X selection=X X X...'.
static std::string StateAsString(const TabStripSelectionModel& model) {
  std::string result = "active=" + base::IntToString(model.active()) +
      " anchor=" + base::IntToString(model.anchor()) +
      " selection=";
  const TabStripSelectionModel::SelectedIndices& selection(
      model.selected_indices());
  for (size_t i = 0; i < selection.size(); ++i) {
    if (i != 0)
      result += " ";
    result += base::IntToString(selection[i]);
  }
  return result;
}

TEST_F(TabStripSelectionModelTest, InitialState) {
  TabStripSelectionModel model;
  EXPECT_EQ("active=-1 anchor=-1 selection=", StateAsString(model));
  EXPECT_TRUE(model.empty());
}

TEST_F(TabStripSelectionModelTest, SetSelectedIndex) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);
  EXPECT_EQ("active=2 anchor=2 selection=2", StateAsString(model));
  EXPECT_FALSE(model.empty());
}

TEST_F(TabStripSelectionModelTest, IncrementFrom) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(1);
  model.IncrementFrom(1);
  EXPECT_EQ("active=2 anchor=2 selection=2", StateAsString(model));

  // Increment from 4. This shouldn't effect the selection as its past the
  // end of the selection.
  model.IncrementFrom(4);
  EXPECT_EQ("active=2 anchor=2 selection=2", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, DecrementFrom) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);
  model.DecrementFrom(0);
  EXPECT_EQ("active=1 anchor=1 selection=1", StateAsString(model));

  // Shift down from 1. As the selection as the index being removed, this should
  // clear the selection.
  model.DecrementFrom(1);
  EXPECT_EQ("active=-1 anchor=-1 selection=", StateAsString(model));

  // Reset the selection to 2, and shift down from 4. This shouldn't do
  // anything.
  model.SetSelectedIndex(2);
  model.DecrementFrom(4);
  EXPECT_EQ("active=2 anchor=2 selection=2", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, IsSelected) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);
  EXPECT_FALSE(model.IsSelected(0));
  EXPECT_TRUE(model.IsSelected(2));
}

TEST_F(TabStripSelectionModelTest, AddIndexToSelected) {
  TabStripSelectionModel model;
  model.AddIndexToSelection(2);
  EXPECT_EQ("active=-1 anchor=-1 selection=2", StateAsString(model));

  model.AddIndexToSelection(4);
  EXPECT_EQ("active=-1 anchor=-1 selection=2 4", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, RemoveIndexFromSelection) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);
  model.AddIndexToSelection(4);
  EXPECT_EQ("active=2 anchor=2 selection=2 4", StateAsString(model));

  model.RemoveIndexFromSelection(4);
  EXPECT_EQ("active=2 anchor=2 selection=2", StateAsString(model));

  model.RemoveIndexFromSelection(2);
  EXPECT_EQ("active=2 anchor=2 selection=", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, Clear) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);

  model.Clear();
  EXPECT_EQ("active=-1 anchor=-1 selection=", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, MoveToLeft) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(0);
  model.AddIndexToSelection(4);
  model.AddIndexToSelection(10);
  model.set_anchor(4);
  model.set_active(4);
  model.Move(4, 0);
  EXPECT_EQ("active=0 anchor=0 selection=0 1 10", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, MoveToRight) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(0);
  model.AddIndexToSelection(4);
  model.AddIndexToSelection(10);
  model.set_anchor(0);
  model.set_active(0);
  model.Move(0, 3);
  EXPECT_EQ("active=3 anchor=3 selection=3 4 10", StateAsString(model));
}

TEST_F(TabStripSelectionModelTest, Copy) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(0);
  model.AddIndexToSelection(4);
  model.AddIndexToSelection(10);
  EXPECT_EQ("active=0 anchor=0 selection=0 4 10", StateAsString(model));
  TabStripSelectionModel model2;
  model2.Copy(model);
  EXPECT_EQ("active=0 anchor=0 selection=0 4 10", StateAsString(model2));
}

TEST_F(TabStripSelectionModelTest, AddSelectionFromAnchorTo) {
  TabStripSelectionModel model;
  model.SetSelectedIndex(2);

  model.AddSelectionFromAnchorTo(4);
  EXPECT_EQ("active=4 anchor=2 selection=2 3 4", StateAsString(model));

  model.AddSelectionFromAnchorTo(0);
  EXPECT_EQ("active=0 anchor=2 selection=0 1 2 3 4", StateAsString(model));
}
