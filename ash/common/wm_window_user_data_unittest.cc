// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_window_user_data.h"

#include <memory>

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_user_data.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace {

// Class that sets a bool* to true from the destructor. Used to track
// destruction.
class Data {
 public:
  explicit Data(bool* delete_setter) : delete_setter_(delete_setter) {}
  ~Data() { *delete_setter_ = true; }

 private:
  bool* delete_setter_;

  DISALLOW_COPY_AND_ASSIGN(Data);
};

}  // namespace

using WmWindowUserDataTest = AshTest;

// Verifies clear() deletes the data associated with a window.
TEST_F(WmWindowUserDataTest, ClearDestroys) {
  WmWindowUserData<Data> user_data;
  WmWindow* window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_UNKNOWN,
                                               ui::LAYER_NOT_DRAWN);
  bool data_deleted = false;
  user_data.Set(window, base::MakeUnique<Data>(&data_deleted));
  EXPECT_FALSE(data_deleted);
  user_data.clear();
  EXPECT_TRUE(data_deleted);
  window->Destroy();
}

// Verifies Set() called with an existing window replaces the existing data.
TEST_F(WmWindowUserDataTest, ReplaceDestroys) {
  WmWindowUserData<Data> user_data;
  WmWindow* window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_UNKNOWN,
                                               ui::LAYER_NOT_DRAWN);
  bool data1_deleted = false;
  user_data.Set(window, base::MakeUnique<Data>(&data1_deleted));
  EXPECT_FALSE(data1_deleted);
  bool data2_deleted = false;
  user_data.Set(window, base::MakeUnique<Data>(&data2_deleted));
  EXPECT_TRUE(data1_deleted);
  EXPECT_FALSE(data2_deleted);
  ASSERT_EQ(1u, user_data.GetWindows().size());
  EXPECT_EQ(window, *user_data.GetWindows().begin());
  window->Destroy();
  EXPECT_TRUE(data2_deleted);
  EXPECT_TRUE(user_data.GetWindows().empty());
}

// Verifies Set() with null deletes existing data.
TEST_F(WmWindowUserDataTest, NullClears) {
  WmWindowUserData<Data> user_data;
  WmWindow* window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_UNKNOWN,
                                               ui::LAYER_NOT_DRAWN);
  bool data1_deleted = false;
  user_data.Set(window, base::MakeUnique<Data>(&data1_deleted));
  EXPECT_FALSE(data1_deleted);
  user_data.Set(window, nullptr);
  EXPECT_TRUE(data1_deleted);
  EXPECT_TRUE(user_data.GetWindows().empty());
  window->Destroy();
}

}  // namespace ash
