// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/extensions/chevron_menu_button.h"

#include "chrome/browser/cocoa/extensions/chevron_menu_button_cell.h"

@implementation ChevronMenuButton

+ (Class)cellClass {
  return [ChevronMenuButtonCell class];
}

@end
