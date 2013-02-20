// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_
#define CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "content/common/content_export.h"
#include "ui/gfx/rect.h"

namespace content {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// the renderer process to the browser process.
struct CONTENT_EXPORT AccessibilityNodeData {
  // An enumeration of accessibility roles.
  enum Role {
    ROLE_UNKNOWN = 0,

    // Used by Chromium to distinguish between the root of the tree
    // for this page, and a web area for a frame within this page.
    ROLE_ROOT_WEB_AREA,

    // These roles all directly correspond to WebKit accessibility roles,
    // keep these alphabetical.
    ROLE_ALERT,
    ROLE_ALERT_DIALOG,
    ROLE_ANNOTATION,
    ROLE_APPLICATION,
    ROLE_ARTICLE,
    ROLE_BROWSER,
    ROLE_BUSY_INDICATOR,
    ROLE_BUTTON,
    ROLE_CANVAS,
    ROLE_CANVAS_WITH_FALLBACK_CONTENT,
    ROLE_CELL,
    ROLE_CHECKBOX,
    ROLE_COLOR_WELL,
    ROLE_COLUMN,
    ROLE_COLUMN_HEADER,
    ROLE_COMBO_BOX,
    ROLE_DEFINITION,
    ROLE_DESCRIPTION_LIST_DETAIL,
    ROLE_DESCRIPTION_LIST_TERM,
    ROLE_DIALOG,
    ROLE_DIRECTORY,
    ROLE_DISCLOSURE_TRIANGLE,
    ROLE_DIV,
    ROLE_DOCUMENT,
    ROLE_DRAWER,
    ROLE_EDITABLE_TEXT,
    ROLE_FOOTER,
    ROLE_FORM,
    ROLE_GRID,
    ROLE_GROUP,
    ROLE_GROW_AREA,
    ROLE_HEADING,
    ROLE_HELP_TAG,
    ROLE_HORIZONTAL_RULE,
    ROLE_IGNORED,
    ROLE_IMAGE,
    ROLE_IMAGE_MAP,
    ROLE_IMAGE_MAP_LINK,
    ROLE_INCREMENTOR,
    ROLE_LABEL,
    ROLE_LANDMARK_APPLICATION,
    ROLE_LANDMARK_BANNER,
    ROLE_LANDMARK_COMPLEMENTARY,
    ROLE_LANDMARK_CONTENTINFO,
    ROLE_LANDMARK_MAIN,
    ROLE_LANDMARK_NAVIGATION,
    ROLE_LANDMARK_SEARCH,
    ROLE_LINK,
    ROLE_LIST,
    ROLE_LISTBOX,
    ROLE_LISTBOX_OPTION,
    ROLE_LIST_ITEM,
    ROLE_LIST_MARKER,
    ROLE_LOG,
    ROLE_MARQUEE,
    ROLE_MATH,
    ROLE_MATTE,
    ROLE_MENU,
    ROLE_MENU_BAR,
    ROLE_MENU_ITEM,
    ROLE_MENU_BUTTON,
    ROLE_MENU_LIST_OPTION,
    ROLE_MENU_LIST_POPUP,
    ROLE_NOTE,
    ROLE_OUTLINE,
    ROLE_PARAGRAPH,
    ROLE_POPUP_BUTTON,
    ROLE_PRESENTATIONAL,
    ROLE_PROGRESS_INDICATOR,
    ROLE_RADIO_BUTTON,
    ROLE_RADIO_GROUP,
    ROLE_REGION,
    ROLE_ROW,
    ROLE_ROW_HEADER,
    ROLE_RULER,
    ROLE_RULER_MARKER,
    ROLE_SCROLLAREA,
    ROLE_SCROLLBAR,
    ROLE_SHEET,
    ROLE_SLIDER,
    ROLE_SLIDER_THUMB,
    ROLE_SPIN_BUTTON,
    ROLE_SPIN_BUTTON_PART,
    ROLE_SPLITTER,
    ROLE_SPLIT_GROUP,
    ROLE_STATIC_TEXT,
    ROLE_STATUS,
    ROLE_SVG_ROOT,
    ROLE_SYSTEM_WIDE,
    ROLE_TAB,
    ROLE_TABLE,
    ROLE_TABLE_HEADER_CONTAINER,
    ROLE_TAB_GROUP_UNUSED,  // WebKit doesn't use (uses ROLE_TAB_LIST)
    ROLE_TAB_LIST,
    ROLE_TAB_PANEL,
    ROLE_TEXTAREA,
    ROLE_TEXT_FIELD,
    ROLE_TIMER,
    ROLE_TOGGLE_BUTTON,
    ROLE_TOOLBAR,
    ROLE_TOOLTIP,
    ROLE_TREE,
    ROLE_TREE_GRID,
    ROLE_TREE_ITEM,
    ROLE_VALUE_INDICATOR,
    ROLE_WEBCORE_LINK,
    ROLE_WEB_AREA,
    ROLE_WINDOW,
    NUM_ROLES
  };

  // An alphabetical enumeration of accessibility states.
  // A state bitmask is formed by shifting 1 to the left by each state,
  // for example:
  //   int mask = (1 << STATE_CHECKED) | (1 << STATE_FOCUSED);
  enum State {
    STATE_BUSY,
    STATE_CHECKED,
    STATE_COLLAPSED,
    STATE_EXPANDED,
    STATE_FOCUSABLE,
    STATE_FOCUSED,
    STATE_HASPOPUP,
    STATE_HOTTRACKED,
    STATE_INDETERMINATE,
    STATE_INVISIBLE,
    STATE_LINKED,
    STATE_MULTISELECTABLE,
    STATE_OFFSCREEN,
    STATE_PRESSED,
    STATE_PROTECTED,
    STATE_READONLY,
    STATE_REQUIRED,
    STATE_SELECTABLE,
    STATE_SELECTED,
    STATE_TRAVERSED,
    STATE_UNAVAILABLE,
    STATE_VERTICAL,
    STATE_VISITED,
    NUM_STATES
  };

  COMPILE_ASSERT(NUM_STATES <= 31, state_enum_not_too_large);

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
    ATTR_LIVE_RELEVANT,
    ATTR_LIVE_STATUS,
    ATTR_ROLE,
    ATTR_SHORTCUT,
    ATTR_URL,
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

    // Table cell attributes.
    ATTR_TABLE_CELL_COLUMN_INDEX,
    ATTR_TABLE_CELL_COLUMN_SPAN,
    ATTR_TABLE_CELL_ROW_INDEX,
    ATTR_TABLE_CELL_ROW_SPAN,

    // Tree control attributes.
    ATTR_HIERARCHICAL_LEVEL,

    // Relationships between this element and other elements.
    ATTR_TITLE_UI_ELEMENT,

    // Color value for ROLE_COLOR_WELL, each component is 0..255
    ATTR_COLOR_VALUE_RED,
    ATTR_COLOR_VALUE_GREEN,
    ATTR_COLOR_VALUE_BLUE
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
  };

  AccessibilityNodeData();
  ~AccessibilityNodeData();

  #ifndef NDEBUG
  std::string DebugString(bool recursive) const;
  #endif

  // This is a simple serializable struct. All member variables should be
  // public and copyable.
  int32 id;
  string16 name;
  string16 value;
  Role role;
  uint32 state;
  gfx::Rect location;
  std::map<StringAttribute, string16> string_attributes;
  std::map<IntAttribute, int32> int_attributes;
  std::map<FloatAttribute, float> float_attributes;
  std::map<BoolAttribute, bool> bool_attributes;
  std::vector<AccessibilityNodeData> children;
  std::vector<int32> indirect_child_ids;
  std::vector<std::pair<string16, string16> > html_attributes;
  std::vector<int32> line_breaks;

  // For a table, the cell ids in row-major order, with duplicate entries
  // when there's a rowspan or colspan, and with -1 for missing cells.
  // There are always exactly rows * columns entries.
  std::vector<int32> cell_ids;

  // For a table, the unique cell ids in row-major order of their first
  // occurrence.
  std::vector<int32> unique_cell_ids;
};

}  // namespace content

#endif  // CONTENT_COMMON_ACCESSIBILITY_NODE_DATA_H_
