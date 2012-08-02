// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"

void DumpAccessibilityTreeHelper::Initialize() {}

string16 DumpAccessibilityTreeHelper::ToString(BrowserAccessibility* node,
                                               char* prefix) {
  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  NSString* dump =
      [NSString stringWithFormat:@"%s%@ "
                                  "subrole=%@ "
                                  "roleDescription='%@' "
                                  "title='%@' "
                                  "value='%@'\n",
       prefix,
       [cocoa_node role],
       [cocoa_node subrole],
       [cocoa_node roleDescription],
       [cocoa_node title],
       [cocoa_node value]];
  std::string tempVal = [dump cStringUsingEncoding:NSUTF8StringEncoding];

  return UTF8ToUTF16(tempVal);
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetActualFileSuffix()
    const {
  return FILE_PATH_LITERAL("-actual-mac.txt");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetExpectedFileSuffix()
    const {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}
