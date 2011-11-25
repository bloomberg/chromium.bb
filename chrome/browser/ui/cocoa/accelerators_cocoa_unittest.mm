// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/singleton.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator_cocoa.h"

TEST(AcceleratorsCocoaTest, GetAccelerator) {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  const ui::AcceleratorCocoa* accelerator =
      keymap->GetAcceleratorForCommand(IDC_COPY);
  ASSERT_TRUE(accelerator);
  EXPECT_NSEQ(@"c", accelerator->characters());
  EXPECT_EQ(static_cast<int>(NSCommandKeyMask), accelerator->modifiers());
}

TEST(AcceleratorsCocoaTest, GetNullAccelerator) {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  const ui::AcceleratorCocoa* accelerator =
      keymap->GetAcceleratorForCommand(314159265);
  EXPECT_FALSE(accelerator);
}
