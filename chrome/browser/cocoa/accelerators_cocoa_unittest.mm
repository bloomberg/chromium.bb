// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/menus/accelerator_cocoa.h"
#include "base/singleton.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/accelerators_cocoa.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

TEST(AcceleratorsCocoaTest, GetAccelerator) {
  AcceleratorsCocoa* keymap = Singleton<AcceleratorsCocoa>::get();
  const menus::AcceleratorCocoa* accelerator =
      keymap->GetAcceleratorForCommand(IDC_COPY);
  ASSERT_TRUE(accelerator);
  EXPECT_NSEQ(@"c", accelerator->characters());
  EXPECT_EQ(static_cast<int>(NSCommandKeyMask), accelerator->modifiers());
}

TEST(AcceleratorsCocoaTest, GetNullAccelerator) {
  AcceleratorsCocoa* keymap = Singleton<AcceleratorsCocoa>::get();
  const menus::AcceleratorCocoa* accelerator =
      keymap->GetAcceleratorForCommand(314159265);
  EXPECT_FALSE(accelerator);
}
