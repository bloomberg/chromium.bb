// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_

#include "base/file_path.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"

// A utility class for retrieving platform specific accessibility information.
class DumpAccessibilityTreeHelper {
 public:
  // Dumps a BrowserAccessibility tree into a string.
  void DumpAccessibilityTree(BrowserAccessibility* node,
                             string16* contents) {
    *contents += ToString(node) + GetLineEnding();
    for (size_t i = 0; i < node->children().size(); ++i)
      DumpAccessibilityTree(node->children()[i], contents);
  }

  // Suffix of the expectation file corresponding to html file.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  // Auto-generated: test-file-actual-mac.txt
  const FilePath::StringType GetActualFileSuffix() const;
  const FilePath::StringType GetExpectedFileSuffix() const;

  // Line ending to use in our dump.
  const string16 GetLineEnding() const;

 protected:
  // Returns a platform specific representation of a BrowserAccessibility.
  string16 ToString(BrowserAccessibility* node);

  void Initialize();
};

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_
