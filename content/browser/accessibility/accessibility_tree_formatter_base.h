// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BASE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BASE_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "ui/gfx/native_widget_types.h"

namespace {
const char kChildrenDictAttr[] = "children";
}

namespace content {

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class CONTENT_EXPORT AccessibilityTreeFormatterBase
    : public AccessibilityTreeFormatter {
 public:
  AccessibilityTreeFormatterBase();
  ~AccessibilityTreeFormatterBase() override;

  // AccessibilityTreeFormatter overrides.
  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;
  std::unique_ptr<base::DictionaryValue> FilterAccessibilityTree(
      const base::DictionaryValue& dict) override;
  void FormatAccessibilityTree(BrowserAccessibility* root,
                               base::string16* contents) override;
  void FormatAccessibilityTree(const base::DictionaryValue& tree_node,
                               base::string16* contents) override;
  void SetPropertyFilters(
      const std::vector<PropertyFilter>& property_filters) override;
  void SetNodeFilters(const std::vector<NodeFilter>& node_filters) override;
  void set_show_ids(bool show_ids) override;
  base::FilePath::StringType GetVersionSpecificExpectedFileSuffix() override;

 protected:
  //
  // Overridden by platform subclasses.
  //

  // Process accessibility tree with filters for output.
  // Given a dictionary that contains a platform-specific dictionary
  // representing an accessibility tree, and utilizing property_filters_ and
  // node_filters_:
  // - Returns a filtered text view as one large string.
  // - Provides a filtered version of the dictionary in an out param,
  //   (only if the out param is provided).
  virtual base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) = 0;

  //
  // Utility functions to be used by each platform.
  //

  base::string16 FormatCoordinates(const base::DictionaryValue& value,
                                   const std::string& name,
                                   const std::string& x_name,
                                   const std::string& y_name);

  base::string16 FormatRectangle(const base::DictionaryValue& value,
                                 const std::string& name,
                                 const std::string& left_name,
                                 const std::string& top_name,
                                 const std::string& width_name,
                                 const std::string& height_name);

  // Writes the given attribute string out to |line| if it matches the property
  // filters.
  // Returns false if the attribute was filtered out.
  bool WriteAttribute(bool include_by_default,
                      const base::string16& attr,
                      base::string16* line);
  bool WriteAttribute(bool include_by_default,
                      const std::string& attr,
                      base::string16* line);
  void AddPropertyFilter(std::vector<PropertyFilter>* property_filters,
                         std::string filter,
                         PropertyFilter::Type type = PropertyFilter::ALLOW);
  bool show_ids() { return show_ids_; }

 private:
  void RecursiveFormatAccessibilityTree(const BrowserAccessibility& node,
                                        base::string16* contents,
                                        int indent);
  void RecursiveFormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                        base::string16* contents,
                                        int depth = 0);

  bool MatchesPropertyFilters(const base::string16& text,
                              bool default_result) const;
  bool MatchesNodeFilters(const base::DictionaryValue& dict) const;

  // Property filters used when formatting the accessibility tree as text.
  // Any property which matches a property filter will be skipped.
  std::vector<PropertyFilter> property_filters_;

  // Node filters used when formatting the accessibility tree as text.
  // Any node which matches a node wilder will be skipped, along with all its
  // children.
  std::vector<NodeFilter> node_filters_;

  // Whether or not node ids should be included in the dump.
  bool show_ids_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeFormatterBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BASE_H_
