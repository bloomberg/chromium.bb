// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_
#define CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/gfx/rect.h"

namespace content {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// the renderer process to the browser process.
struct CONTENT_EXPORT AccessibilityNodeData {
  // Additional optional attributes that can be optionally attached to
  // a node.
  enum StringAttribute {
    // Document attributes.
    ATTR_DOC_URL,
    ATTR_DOC_TITLE,
    ATTR_DOC_MIMETYPE,
    ATTR_DOC_DOCTYPE,

    // Attributes that could apply to any node.
    ATTR_ACCESS_KEY,
    ATTR_ACTION,
    ATTR_CONTAINER_LIVE_RELEVANT,
    ATTR_CONTAINER_LIVE_STATUS,
    ATTR_DESCRIPTION,
    ATTR_DISPLAY,
    ATTR_HELP,
    ATTR_HTML_TAG,
    ATTR_NAME,
    ATTR_LIVE_RELEVANT,
    ATTR_LIVE_STATUS,
    ATTR_ROLE,
    ATTR_SHORTCUT,
    ATTR_URL,
    ATTR_VALUE,
  };

  enum IntAttribute {
    // Scrollable container attributes.
    ATTR_SCROLL_X,
    ATTR_SCROLL_X_MIN,
    ATTR_SCROLL_X_MAX,
    ATTR_SCROLL_Y,
    ATTR_SCROLL_Y_MIN,
    ATTR_SCROLL_Y_MAX,

    // Editable text attributes.
    ATTR_TEXT_SEL_START,
    ATTR_TEXT_SEL_END,

    // Table attributes.
    ATTR_TABLE_ROW_COUNT,
    ATTR_TABLE_COLUMN_COUNT,
    ATTR_TABLE_HEADER_ID,

    // Table row attributes.
    ATTR_TABLE_ROW_INDEX,
    ATTR_TABLE_ROW_HEADER_ID,

    // Table column attributes.
    ATTR_TABLE_COLUMN_INDEX,
    ATTR_TABLE_COLUMN_HEADER_ID,

    // Table cell attributes.
    ATTR_TABLE_CELL_COLUMN_INDEX,
    ATTR_TABLE_CELL_COLUMN_SPAN,
    ATTR_TABLE_CELL_ROW_INDEX,
    ATTR_TABLE_CELL_ROW_SPAN,

    // Tree control attributes.
    ATTR_HIERARCHICAL_LEVEL,

    // Relationships between this element and other elements.
    ATTR_TITLE_UI_ELEMENT,

    // Color value for blink::WebAXRoleColorWell, each component is 0..255
    ATTR_COLOR_VALUE_RED,
    ATTR_COLOR_VALUE_GREEN,
    ATTR_COLOR_VALUE_BLUE,

    // Inline text attributes.
    ATTR_TEXT_DIRECTION
  };

  enum FloatAttribute {
    // Document attributes.
    ATTR_DOC_LOADING_PROGRESS,

    // Range attributes.
    ATTR_VALUE_FOR_RANGE,
    ATTR_MIN_VALUE_FOR_RANGE,
    ATTR_MAX_VALUE_FOR_RANGE,
  };

  enum BoolAttribute {
    // Document attributes.
    ATTR_DOC_LOADED,

    // True if a checkbox or radio button is in the "mixed" state.
    ATTR_BUTTON_MIXED,

    // Live region attributes.
    ATTR_CONTAINER_LIVE_ATOMIC,
    ATTR_CONTAINER_LIVE_BUSY,
    ATTR_LIVE_ATOMIC,
    ATTR_LIVE_BUSY,

    // ARIA readonly flag.
    ATTR_ARIA_READONLY,

    // Writeable attributes
    ATTR_CAN_SET_VALUE,

    // If this is set, all of the other fields in this struct should
    // be ignored and only the locations should change.
    ATTR_UPDATE_LOCATION_ONLY,

    // Set on a canvas element if it has fallback content.
    ATTR_CANVAS_HAS_FALLBACK,
  };

  enum IntListAttribute {
    // Ids of nodes that are children of this node logically, but are
    // not children of this node in the tree structure. As an example,
    // a table cell is a child of a row, and an 'indirect' child of a
    // column.
    ATTR_INDIRECT_CHILD_IDS,

    // Character indices where line breaks occur.
    ATTR_LINE_BREAKS,

    // For a table, the cell ids in row-major order, with duplicate entries
    // when there's a rowspan or colspan, and with -1 for missing cells.
    // There are always exactly rows * columns entries.
    ATTR_CELL_IDS,

    // For a table, the unique cell ids in row-major order of their first
    // occurrence.
    ATTR_UNIQUE_CELL_IDS,

    // For inline text. This is the pixel position of the end of this
    // character within the bounding rectangle of this object, in the
    // direction given by ATTR_TEXT_DIRECTION. For example, for left-to-right
    // text, the first offset is the right coordinate of the first character
    // within the object's bounds, the second offset is the right coordinate
    // of the second character, and so on.
    ATTR_CHARACTER_OFFSETS,

    // For inline text. These int lists must be the same size; they represent
    // the start and end character index of each word within this text.
    ATTR_WORD_STARTS,
    ATTR_WORD_ENDS,
  };

  AccessibilityNodeData();
  virtual ~AccessibilityNodeData();

  void AddStringAttribute(StringAttribute attribute,
                          const std::string& value);
  void AddIntAttribute(IntAttribute attribute, int value);
  void AddFloatAttribute(FloatAttribute attribute, float value);
  void AddBoolAttribute(BoolAttribute attribute, bool value);
  void AddIntListAttribute(IntListAttribute attribute,
                           const std::vector<int32>& value);

  // Convenience functions, mainly for writing unit tests.
  // Equivalent to AddStringAttribute(ATTR_NAME, name).
  void SetName(std::string name);
  // Equivalent to AddStringAttribute(ATTR_VALUE, value).
  void SetValue(std::string value);

  #ifndef NDEBUG
  virtual std::string DebugString(bool recursive) const;
  #endif

  // This is a simple serializable struct. All member variables should be
  // public and copyable.
  int32 id;
  blink::WebAXRole role;
  uint32 state;
  gfx::Rect location;
  std::vector<std::pair<StringAttribute, std::string> > string_attributes;
  std::vector<std::pair<IntAttribute, int32> > int_attributes;
  std::vector<std::pair<FloatAttribute, float> > float_attributes;
  std::vector<std::pair<BoolAttribute, bool> > bool_attributes;
  std::vector<std::pair<IntListAttribute, std::vector<int32> > >
      intlist_attributes;
  std::vector<std::pair<std::string, std::string> > html_attributes;
  std::vector<int32> child_ids;
};

// For testing and debugging only: this subclass of AccessibilityNodeData
// is used to represent a whole tree of accessibility nodes, where each
// node owns its children. This makes it easy to print the tree structure
// or search it recursively.
struct CONTENT_EXPORT AccessibilityNodeDataTreeNode
    : public AccessibilityNodeData {
  AccessibilityNodeDataTreeNode();
  virtual ~AccessibilityNodeDataTreeNode();

  AccessibilityNodeDataTreeNode& operator=(const AccessibilityNodeData& src);

  #ifndef NDEBUG
  virtual std::string DebugString(bool recursive) const OVERRIDE;
  #endif

  std::vector<AccessibilityNodeDataTreeNode> children;
};

// Given a vector of accessibility nodes that represent a complete
// accessibility tree, where each node appears before its children,
// build a tree of AccessibilityNodeDataTreeNode objects for easier
// testing and debugging, where each node contains its children.
// The |dst| argument will become the root of the new tree.
void MakeAccessibilityNodeDataTree(
    const std::vector<AccessibilityNodeData>& src,
    AccessibilityNodeDataTreeNode* dst);

}  // namespace content

#endif  // CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_
