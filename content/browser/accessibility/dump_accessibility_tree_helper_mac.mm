// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

namespace {

string16 Format(BrowserAccessibility* node,
                const char *prefix,
                SEL selector,
                const char *suffix) {
  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  id value = [cocoa_node performSelector:selector];
  if (!value)
    return string16();
  NSString* format_str =
      [NSString stringWithFormat:@"%s%%@%s", prefix, suffix];
  NSString* tmp = [NSString stringWithFormat:format_str, value];
  return UTF8ToUTF16([tmp cStringUsingEncoding:NSUTF8StringEncoding]);
}

string16 FormatPosition(BrowserAccessibility* node) {
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and less
  // confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  BrowserAccessibility* root = node->manager()->GetRoot();
  BrowserAccessibilityCocoa* cocoa_root = root->ToBrowserAccessibilityCocoa();
  NSPoint root_position = [[cocoa_root position] pointValue];
  NSSize root_size = [[cocoa_root size] sizeValue];
  int root_top = -static_cast<int>(root_position.y + root_size.height);
  int root_left = static_cast<int>(root_position.x);

  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  NSPoint node_position = [[cocoa_node position] pointValue];
  NSSize node_size = [[cocoa_node size] sizeValue];

  NSString* position_str =
      [NSString stringWithFormat:@"position=(%d, %d)",
                static_cast<int>(node_position.x - root_left),
                static_cast<int>(
                    -node_position.y - node_size.height - root_top)];
  return UTF8ToUTF16([position_str cStringUsingEncoding:NSUTF8StringEncoding]);
}

string16 FormatSize(BrowserAccessibility* node) {
  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  NSSize node_size = [[cocoa_node size] sizeValue];
  NSString* size_str =
      [NSString stringWithFormat:@"size=(%d, %d)",
                static_cast<int>(node_size.width),
                static_cast<int>(node_size.height)];
  return UTF8ToUTF16([size_str cStringUsingEncoding:NSUTF8StringEncoding]);
}

}  // namespace

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
  Add(false, Format(node, "minValue='", @selector(minValue), "'"));
  Add(false, Format(node, "maxValue='", @selector(maxValue), "'"));
  Add(false, Format(node, "valueDescription='", @selector(valueDescription),
                    "'"));
  Add(false, Format(node, "description='", @selector(description), "'"));
  Add(false, Format(node, "help='", @selector(help), "'"));
  Add(false, Format(node, "invalid='", @selector(invalid), "'"));
  Add(false, Format(node, "disclosing='", @selector(disclosing), "'"));
  Add(false, Format(node, "disclosureLevel='", @selector(disclosureLevel),
                    "'"));
  Add(false, Format(node, "accessKey='", @selector(accessKey), "'"));
  Add(false, Format(node, "ariaAtomic='", @selector(ariaAtomic), "'"));
  Add(false, Format(node, "ariaBusy='", @selector(ariaBusy), "'"));
  Add(false, Format(node, "ariaLive='", @selector(ariaLive), "'"));
  Add(false, Format(node, "ariaRelevant='", @selector(ariaRelevant), "'"));
  Add(false, Format(node, "enabled='", @selector(enabled), "'"));
  Add(false, Format(node, "focused='", @selector(focused), "'"));
  Add(false, Format(node, "loaded='", @selector(loaded), "'"));
  Add(false, Format(node, "loadingProgress='", @selector(loadingProgress),
                    "'"));
  Add(false, Format(node, "numberOfCharacters='",
                    @selector(numberOfCharacters), "'"));
  Add(false, Format(node, "orientation='", @selector(orientation), "'"));
  Add(false, Format(node, "required='", @selector(required), "'"));
  Add(false, Format(node, "url='", @selector(url), "'"));
  Add(false, Format(node, "visibleCharacterRange='",
                    @selector(visibleCharacterRange), "'"));
  Add(false, Format(node, "visited='", @selector(visited), "'"));
  Add(false, FormatPosition(node));
  Add(false, FormatSize(node));

  return ASCIIToUTF16(prefix) + FinishLine() + ASCIIToUTF16("\n");
}

const base::FilePath::StringType
DumpAccessibilityTreeHelper::GetActualFileSuffix()
    const {
  return FILE_PATH_LITERAL("-actual-mac.txt");
}

const base::FilePath::StringType
DumpAccessibilityTreeHelper::GetExpectedFileSuffix()
    const {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

const std::string DumpAccessibilityTreeHelper::GetAllowEmptyString() const {
  return "@MAC-ALLOW-EMPTY:";
}

const std::string DumpAccessibilityTreeHelper::GetAllowString() const {
  return "@MAC-ALLOW:";
}

const std::string DumpAccessibilityTreeHelper::GetDenyString() const {
  return "@MAC-DENY:";
}

}  // namespace content
