// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_

#include <vector>

#include "base/file_path.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"

namespace content {

// A utility class for retrieving platform specific accessibility information.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class DumpAccessibilityTreeHelper {
 public:
  DumpAccessibilityTreeHelper();
  virtual ~DumpAccessibilityTreeHelper();

  // Dumps a BrowserAccessibility tree into a string.
  void DumpAccessibilityTree(BrowserAccessibility* node,
                             string16* contents);

  // A single filter specification. See GetAllowString() and GetDenyString()
  // for more information.
  struct Filter {
    enum Type {
      ALLOW,
      DENY
    };
    string16 match_str;
    Type type;

    Filter(string16 match_str, Type type)
        : match_str(match_str), type(type) {}
  };

  // Set regular expression filters that apply to each component of every
  // line before it's output.
  void SetFilters(const std::vector<Filter>& filters);

  // Suffix of the expectation file corresponding to html file.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  // Auto-generated: test-file-actual-mac.txt
  const FilePath::StringType GetActualFileSuffix() const;
  const FilePath::StringType GetExpectedFileSuffix() const;

  // A platform-specific string that indicates a given line in a file
  // is an allow or deny filter. Example:
  // Mac values:
  //   GetAllowString() -> "@MAC-ALLOW:"
  //   GetDenyString() -> "@MAC-DENY:"
  // Example html:
  // <!--
  // @MAC-ALLOW:roleDescription*
  // @MAC-DENY:subrole*
  // -->
  // <p>Text</p>
  const std::string GetAllowString() const;
  const std::string GetDenyString() const;

 protected:
  void RecursiveDumpAccessibilityTree(BrowserAccessibility* node,
                                      string16* contents,
                                      int indent);

  // Returns a platform specific representation of a BrowserAccessibility.
  // Should be zero or more complete lines, each with |prefix| prepended
  // (to indent each line).
  string16 ToString(BrowserAccessibility* node, char* prefix);

  void Initialize();

  bool MatchesFilters(const string16& text, bool default_result);
  void StartLine();
  void Add(bool include_by_default, const string16& attr);
  string16 FinishLine();

  std::vector<Filter> filters_;
  string16 line_;

  DISALLOW_COPY_AND_ASSIGN(DumpAccessibilityTreeHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_HELPER_H_
