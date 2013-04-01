// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/singleton.h"
#include "chrome/app/chrome_command_ids.h"
#import "ui/base/accelerators/platform_accelerator_cocoa.h"

namespace {

const struct AcceleratorMapping {
  int command_id;
  NSString* key;
  NSUInteger modifiers;
} kAcceleratorMap[] = {
  { IDC_CLEAR_BROWSING_DATA, @"\x8", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_COPY, @"c", NSCommandKeyMask },
  { IDC_CUT, @"x", NSCommandKeyMask },
  { IDC_DEV_TOOLS, @"i", NSCommandKeyMask | NSAlternateKeyMask },
  { IDC_DEV_TOOLS_CONSOLE, @"j", NSCommandKeyMask | NSAlternateKeyMask },
  { IDC_FIND, @"f", NSCommandKeyMask },
  { IDC_FULLSCREEN, @"f", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_NEW_INCOGNITO_WINDOW, @"n", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_NEW_TAB, @"t", NSCommandKeyMask },
  { IDC_NEW_WINDOW, @"n", NSCommandKeyMask },
  { IDC_OPTIONS, @",", NSCommandKeyMask },
  { IDC_PASTE, @"v", NSCommandKeyMask },
  { IDC_PRINT, @"p", NSCommandKeyMask },
  { IDC_RESTORE_TAB, @"t", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_SAVE_PAGE, @"s", NSCommandKeyMask },
  { IDC_SHOW_BOOKMARK_BAR, @"b", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_SHOW_BOOKMARK_MANAGER, @"b", NSCommandKeyMask | NSAlternateKeyMask },
  { IDC_BOOKMARK_PAGE, @"d", NSCommandKeyMask },
  { IDC_SHOW_DOWNLOADS, @"j", NSCommandKeyMask | NSShiftKeyMask },
  { IDC_SHOW_HISTORY, @"y", NSCommandKeyMask },
  { IDC_VIEW_SOURCE, @"u", NSCommandKeyMask | NSAlternateKeyMask },
  { IDC_ZOOM_MINUS, @"-", NSCommandKeyMask },
  { IDC_ZOOM_PLUS, @"+", NSCommandKeyMask }
};

}  // namespace

AcceleratorsCocoa::AcceleratorsCocoa() {
  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    const AcceleratorMapping& entry = kAcceleratorMap[i];
    ui::Accelerator accelerator(ui::VKEY_UNKNOWN, 0);
    scoped_ptr<ui::PlatformAccelerator> platform_accelerator(
        new ui::PlatformAcceleratorCocoa(entry.key, entry.modifiers));
    accelerator.set_platform_accelerator(platform_accelerator.Pass());
    accelerators_.insert(std::make_pair(entry.command_id, accelerator));
  }
}

AcceleratorsCocoa::~AcceleratorsCocoa() {}

// static
AcceleratorsCocoa* AcceleratorsCocoa::GetInstance() {
  return Singleton<AcceleratorsCocoa>::get();
}

const ui::Accelerator* AcceleratorsCocoa::GetAcceleratorForCommand(
    int command_id) {
  AcceleratorMap::iterator it = accelerators_.find(command_id);
  if (it == accelerators_.end())
    return NULL;
  return &it->second;
}
