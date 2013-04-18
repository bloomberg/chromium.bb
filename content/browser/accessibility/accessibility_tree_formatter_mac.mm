// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

namespace {

string16 Format(const char *prefix,
                id value,
                const char *suffix) {
  if (value == nil)
    return UTF8ToUTF16("");
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

void AccessibilityTreeFormatter::Initialize() {}

string16 AccessibilityTreeFormatter::ToString(BrowserAccessibility* node,
                                               char* prefix) {
  StartLine();
  NSArray* requestedAttributes = [NSArray arrayWithObjects:
      NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityMinValueAttribute,
      NSAccessibilityMaxValueAttribute,
      NSAccessibilityValueDescriptionAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityHelpAttribute,
      @"AXInvalid",
      NSAccessibilityDisclosingAttribute,
      NSAccessibilityDisclosureLevelAttribute,
      @"AXAccessKey",
      @"AXARIAAtomic",
      @"AXARIABusy",
      @"AXARIALive",
      @"AXARIARelevant",
      NSAccessibilityEnabledAttribute,
      NSAccessibilityFocusedAttribute,
      @"AXLoaded",
      @"AXLoadingProcess",
      NSAccessibilityNumberOfCharactersAttribute,
      NSAccessibilityOrientationAttribute,
      @"AXRequired",
      NSAccessibilityURLAttribute,
      NSAccessibilityVisibleCharacterRangeAttribute,
      @"AXVisited",
      nil];

  NSArray* defaultAttributes = [NSArray arrayWithObjects:
      NSAccessibilityTitleAttribute,
      NSAccessibilityValueAttribute,
      nil];

  BrowserAccessibilityCocoa* cocoa_node = node->ToBrowserAccessibilityCocoa();
  NSArray* supportedAttributes = [cocoa_node accessibilityAttributeNames];

  Add(true,
      Format("", [cocoa_node accessibilityAttributeValue:
                      NSAccessibilityRoleAttribute],
             ""));
  Add(false,
      Format("subrole=", [cocoa_node accessibilityAttributeValue:
                              NSAccessibilitySubroleAttribute],
             ""));
  for (NSString* requestedAttribute in requestedAttributes) {
    if (![supportedAttributes containsObject:requestedAttribute]) {
      continue;
    }
    NSString* methodName =
        [cocoa_node methodNameForAttribute:requestedAttribute];
    Add([defaultAttributes containsObject:requestedAttribute],
        Format([[NSString stringWithFormat:@"%@='", methodName]
                   cStringUsingEncoding:NSUTF8StringEncoding],
               [cocoa_node accessibilityAttributeValue:
                    requestedAttribute],
               "'"));
  }
  Add(false, FormatPosition(node));
  Add(false, FormatSize(node));

  return ASCIIToUTF16(prefix) + FinishLine() + ASCIIToUTF16("\n");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("-actual-mac.txt");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "@MAC-ALLOW-EMPTY:";
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return "@MAC-ALLOW:";
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return "@MAC-DENY:";
}

}  // namespace content
