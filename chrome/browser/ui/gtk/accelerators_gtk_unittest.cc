// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkkeysyms.h>

#include "base/memory/singleton.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator_gtk.h"

TEST(AcceleratorsGtkTest, GetAccelerator) {
  AcceleratorsGtk* keymap = AcceleratorsGtk::GetInstance();
  const ui::AcceleratorGtk* accelerator =
      keymap->GetPrimaryAcceleratorForCommand(IDC_COPY);
  ASSERT_TRUE(accelerator);
  EXPECT_EQ(static_cast<guint>(GDK_c), accelerator->GetGdkKeyCode());
  EXPECT_EQ(GDK_CONTROL_MASK, accelerator->modifiers());
}

TEST(AcceleratorsGtkTest, GetNullAccelerator) {
  AcceleratorsGtk* keymap = AcceleratorsGtk::GetInstance();
  const ui::AcceleratorGtk* accelerator =
      keymap->GetPrimaryAcceleratorForCommand(IDC_MinimumLabelValue - 1);
  EXPECT_FALSE(accelerator);
}
