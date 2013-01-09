// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"

namespace content {
namespace {
string16 Format(BrowserAccessibility* node,
                const char *prefix,
                SEL selector,
                const char *suffix) {
  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  NSString* format_str =
      [NSString stringWithFormat:@"%s%%@%s", prefix, suffix];
  NSString* tmp = [NSString stringWithFormat:format_str,
                   [cocoa_node performSelector:selector]];
  return UTF8ToUTF16([tmp cStringUsingEncoding:NSUTF8StringEncoding]);
}
}

void DumpAccessibilityTreeHelper::Initialize() {}

string16 DumpAccessibilityTreeHelper::ToString(BrowserAccessibility* node,
                                               char* prefix) {
  StartLine();
  Add(true, Format(node, "", @selector(role), ""));
  Add(false, Format(node, "subrole=", @selector(subrole), ""));
  Add(false, Format(node, "roleDescription='",
                          @selector(roleDescription),
                          "'"));
  Add(true, Format(node, "title='", @selector(title), "'"));
  Add(true, Format(node, "value='", @selector(value), "'"));
  Add(false, Format(node, "description='", @selector(description), "'"));
  Add(false, Format(node, "help='", @selector(help), "'"));
  return ASCIIToUTF16(prefix) + FinishLine() + ASCIIToUTF16("\n");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetActualFileSuffix()
    const {
  return FILE_PATH_LITERAL("-actual-mac.txt");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetExpectedFileSuffix()
    const {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

const std::string DumpAccessibilityTreeHelper::GetAllowString() const {
  return "@MAC-ALLOW:";
}

const std::string DumpAccessibilityTreeHelper::GetDenyString() const {
  return "@MAC-DENY:";
}

}  // namespace content
