// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/x11_input_method_context_impl_gtk2.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace libgtk2ui {

class X11InputMethodContextImplGtk2FriendTest : public testing::Test {
};

TEST_F(X11InputMethodContextImplGtk2FriendTest, GtkCommitSignalTrap) {
  libgtk2ui::X11InputMethodContextImplGtk2::GtkCommitSignalTrap trap;

  // Test the initial state.
  EXPECT_FALSE(trap.IsSignalCaught());
  EXPECT_FALSE(trap.Trap(base::string16()));
  EXPECT_FALSE(trap.IsSignalCaught());

  // Cases which don't match the target keyval.
  trap.StartTrap('t');
  EXPECT_FALSE(trap.Trap(base::UTF8ToUTF16("T")));
  EXPECT_FALSE(trap.IsSignalCaught());
  EXPECT_FALSE(trap.Trap(base::UTF8ToUTF16("true")));
  EXPECT_FALSE(trap.IsSignalCaught());

  // Do not catch when the trap is not activated.
  trap.StopTrap();
  EXPECT_FALSE(trap.Trap(base::UTF8ToUTF16("t")));
  EXPECT_FALSE(trap.IsSignalCaught());

  // Successive calls don't break anything.
  trap.StopTrap();
  trap.StopTrap();
  EXPECT_FALSE(trap.Trap(base::UTF8ToUTF16("t")));
  EXPECT_FALSE(trap.IsSignalCaught());
  trap.StartTrap('f');
  trap.StartTrap('t');
  EXPECT_TRUE(trap.Trap(base::UTF8ToUTF16("t")));
  EXPECT_TRUE(trap.IsSignalCaught());

  // StartTrap() resets the state.
  trap.StartTrap('t');
  EXPECT_FALSE(trap.IsSignalCaught());
  // Many times calls to Trap() are okay.
  EXPECT_FALSE(trap.Trap(base::UTF8ToUTF16("f")));
  EXPECT_FALSE(trap.IsSignalCaught());
  EXPECT_TRUE(trap.Trap(base::UTF8ToUTF16("t")));
  EXPECT_TRUE(trap.IsSignalCaught());
}

}  // namespace libgtk2ui
