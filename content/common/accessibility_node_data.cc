// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/accessibility_node_data.h"

#include <set>

#include "base/containers/hash_tables.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using base::DoubleToString;
using base::IntToString;

namespace {

#ifndef NDEBUG
std::string IntVectorToString(const std::vector<int>& items) {
  std::string str;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      str += ",";
    str += IntToString(items[i]);
  }
  return str;
}
#endif

}  // Anonymous namespace

namespace content {

AccessibilityNodeData::AccessibilityNodeData()
    : id(-1),
      role(WebKit::WebAXRoleUnknown),
      state(-1) {
}

AccessibilityNodeData::~AccessibilityNodeData() {
}

void AccessibilityNodeData::AddStringAttribute(
    StringAttribute attribute, const std::string& value) {
  string_attributes.push_back(std::make_pair(attribute, value));
}

void AccessibilityNodeData::AddIntAttribute(
    IntAttribute attribute, int value) {
  int_attributes.push_back(std::make_pair(attribute, value));
}

void AccessibilityNodeData::AddFloatAttribute(
    FloatAttribute attribute, float value) {
  float_attributes.push_back(std::make_pair(attribute, value));
}

void AccessibilityNodeData::AddBoolAttribute(
    BoolAttribute attribute, bool value) {
  bool_attributes.push_back(std::make_pair(attribute, value));
}

void AccessibilityNodeData::AddIntListAttribute(
    IntListAttribute attribute, const std::vector<int32>& value) {
  intlist_attributes.push_back(std::make_pair(attribute, value));
}

void AccessibilityNodeData::SetName(std::string name) {
  string_attributes.push_back(std::make_pair(ATTR_NAME, name));
}

void AccessibilityNodeData::SetValue(std::string value) {
  string_attributes.push_back(std::make_pair(ATTR_VALUE, value));
}

AccessibilityNodeDataTreeNode::AccessibilityNodeDataTreeNode()
  : AccessibilityNodeData() {
}

AccessibilityNodeDataTreeNode::~AccessibilityNodeDataTreeNode() {
}

AccessibilityNodeDataTreeNode& AccessibilityNodeDataTreeNode::operator=(
    const AccessibilityNodeData& src) {
  AccessibilityNodeData::operator=(src);
  return *this;
}

void MakeAccessibilityNodeDataTree(
    const std::vector<AccessibilityNodeData>& src_vector,
    AccessibilityNodeDataTreeNode* dst_root) {
  // This method assumes |src_vector| contains all of the nodes of
  // an accessibility tree, and that each parent comes before its
  // children. Each node has an id, and the ids of its children.
  // The output is a tree where each node contains its children.

  // Initialize a hash map with all of the ids in |src_vector|.
  base::hash_map<int32, AccessibilityNodeDataTreeNode*> id_map;
  for (size_t i = 0; i < src_vector.size(); ++i)
    id_map[src_vector[i].id] = NULL;

  // Copy the nodes to the output tree one at a time.
  for (size_t i = 0; i < src_vector.size(); ++i) {
    const AccessibilityNodeData& src_node = src_vector[i];
    AccessibilityNodeDataTreeNode* dst_node;

    // If it's the first element in the vector, assume it's
    // the root.  For any other element, look for it in our
    // hash map, and skip it if not there (meaning there was
    // an extranous node, or the nodes were sent in the wrong
    // order).
    if (i == 0) {
      dst_node = dst_root;
    } else {
      dst_node = id_map[src_node.id];
      if (!dst_node)
        continue;
    }

    // Copy the node data.
    *dst_node = src_node;

    // Add placeholders for all of the node's children in the tree,
    // and add them to the hash map so we can find them when we
    // encounter them in |src_vector|.
    dst_node->children.reserve(src_node.child_ids.size());
    for (size_t j = 0; j < src_node.child_ids.size(); ++j) {
      int child_id = src_node.child_ids[j];
      if (id_map.find(child_id) != id_map.end()) {
        dst_node->children.push_back(AccessibilityNodeDataTreeNode());
        id_map[child_id] = &dst_node->children.back();
      }
    }
  }
}

#ifndef NDEBUG
std::string AccessibilityNodeData::DebugString(bool recursive) const {
  std::string result;

  result += "id=" + IntToString(id);

  switch (role) {
    case WebKit::WebAXRoleAlert: result += " ALERT"; break;
    case WebKit::WebAXRoleAlertDialog: result += " ALERT_DIALOG"; break;
    case WebKit::WebAXRoleAnnotation: result += " ANNOTATION"; break;
    case WebKit::WebAXRoleApplication: result += " APPLICATION"; break;
    case WebKit::WebAXRoleArticle: result += " ARTICLE"; break;
    case WebKit::WebAXRoleBanner: result += " L_BANNER"; break;
    case WebKit::WebAXRoleBrowser: result += " BROWSER"; break;
    case WebKit::WebAXRoleBusyIndicator: result += " BUSY_INDICATOR"; break;
    case WebKit::WebAXRoleButton: result += " BUTTON"; break;
    case WebKit::WebAXRoleCanvas: result += " CANVAS"; break;
    case WebKit::WebAXRoleCell: result += " CELL"; break;
    case WebKit::WebAXRoleCheckBox: result += " CHECKBOX"; break;
    case WebKit::WebAXRoleColorWell: result += " COLOR_WELL"; break;
    case WebKit::WebAXRoleColumn: result += " COLUMN"; break;
    case WebKit::WebAXRoleColumnHeader: result += " COLUMN_HEADER"; break;
    case WebKit::WebAXRoleComboBox: result += " COMBO_BOX"; break;
    case WebKit::WebAXRoleComplementary: result += " L_COMPLEMENTARY"; break;
    case WebKit::WebAXRoleContentInfo: result += " L_CONTENTINFO"; break;
    case WebKit::WebAXRoleDefinition: result += " DEFINITION"; break;
    case WebKit::WebAXRoleDescriptionListDetail: result += " DD"; break;
    case WebKit::WebAXRoleDescriptionListTerm: result += " DT"; break;
    case WebKit::WebAXRoleDialog: result += " DIALOG"; break;
    case WebKit::WebAXRoleDirectory: result += " DIRECTORY"; break;
    case WebKit::WebAXRoleDisclosureTriangle:
        result += " DISCLOSURE_TRIANGLE"; break;
    case WebKit::WebAXRoleDiv: result += " DIV"; break;
    case WebKit::WebAXRoleDocument: result += " DOCUMENT"; break;
    case WebKit::WebAXRoleDrawer: result += " DRAWER"; break;
    case WebKit::WebAXRoleEditableText: result += " EDITABLE_TEXT"; break;
    case WebKit::WebAXRoleFooter: result += " FOOTER"; break;
    case WebKit::WebAXRoleForm: result += " FORM"; break;
    case WebKit::WebAXRoleGrid: result += " GRID"; break;
    case WebKit::WebAXRoleGroup: result += " GROUP"; break;
    case WebKit::WebAXRoleGrowArea: result += " GROW_AREA"; break;
    case WebKit::WebAXRoleHeading: result += " HEADING"; break;
    case WebKit::WebAXRoleHelpTag: result += " HELP_TAG"; break;
    case WebKit::WebAXRoleHorizontalRule: result += " HORIZONTAL_RULE"; break;
    case WebKit::WebAXRoleIgnored: result += " IGNORED"; break;
    case WebKit::WebAXRoleImage: result += " IMAGE"; break;
    case WebKit::WebAXRoleImageMap: result += " IMAGE_MAP"; break;
    case WebKit::WebAXRoleImageMapLink: result += " IMAGE_MAP_LINK"; break;
    case WebKit::WebAXRoleIncrementor: result += " INCREMENTOR"; break;
    case WebKit::WebAXRoleInlineTextBox: result += " INLINE_TEXT_BOX"; break;
    case WebKit::WebAXRoleLabel: result += " LABEL"; break;
    case WebKit::WebAXRoleLink: result += " LINK"; break;
    case WebKit::WebAXRoleList: result += " LIST"; break;
    case WebKit::WebAXRoleListBox: result += " LISTBOX"; break;
    case WebKit::WebAXRoleListBoxOption: result += " LISTBOX_OPTION"; break;
    case WebKit::WebAXRoleListItem: result += " LIST_ITEM"; break;
    case WebKit::WebAXRoleListMarker: result += " LIST_MARKER"; break;
    case WebKit::WebAXRoleLog: result += " LOG"; break;
    case WebKit::WebAXRoleMain: result += " L_MAIN"; break;
    case WebKit::WebAXRoleMarquee: result += " MARQUEE"; break;
    case WebKit::WebAXRoleMath: result += " MATH"; break;
    case WebKit::WebAXRoleMatte: result += " MATTE"; break;
    case WebKit::WebAXRoleMenu: result += " MENU"; break;
    case WebKit::WebAXRoleMenuBar: result += " MENU_BAR"; break;
    case WebKit::WebAXRoleMenuButton: result += " MENU_BUTTON"; break;
    case WebKit::WebAXRoleMenuItem: result += " MENU_ITEM"; break;
    case WebKit::WebAXRoleMenuListOption: result += " MENU_LIST_OPTION"; break;
    case WebKit::WebAXRoleMenuListPopup: result += " MENU_LIST_POPUP"; break;
    case WebKit::WebAXRoleNavigation: result += " L_NAVIGATION"; break;
    case WebKit::WebAXRoleNote: result += " NOTE"; break;
    case WebKit::WebAXRoleOutline: result += " OUTLINE"; break;
    case WebKit::WebAXRoleParagraph: result += " PARAGRAPH"; break;
    case WebKit::WebAXRolePopUpButton: result += " POPUP_BUTTON"; break;
    case WebKit::WebAXRolePresentational: result += " PRESENTATIONAL"; break;
    case WebKit::WebAXRoleProgressIndicator:
        result += " PROGRESS_INDICATOR"; break;
    case WebKit::WebAXRoleRadioButton: result += " RADIO_BUTTON"; break;
    case WebKit::WebAXRoleRadioGroup: result += " RADIO_GROUP"; break;
    case WebKit::WebAXRoleRegion: result += " REGION"; break;
    case WebKit::WebAXRoleRootWebArea: result += " ROOT_WEB_AREA"; break;
    case WebKit::WebAXRoleRow: result += " ROW"; break;
    case WebKit::WebAXRoleRowHeader: result += " ROW_HEADER"; break;
    case WebKit::WebAXRoleRuler: result += " RULER"; break;
    case WebKit::WebAXRoleRulerMarker: result += " RULER_MARKER"; break;
    case WebKit::WebAXRoleSVGRoot: result += " SVG_ROOT"; break;
    case WebKit::WebAXRoleScrollArea: result += " SCROLLAREA"; break;
    case WebKit::WebAXRoleScrollBar: result += " SCROLLBAR"; break;
    case WebKit::WebAXRoleSearch: result += " L_SEARCH"; break;
    case WebKit::WebAXRoleSheet: result += " SHEET"; break;
    case WebKit::WebAXRoleSlider: result += " SLIDER"; break;
    case WebKit::WebAXRoleSliderThumb: result += " SLIDER_THUMB"; break;
    case WebKit::WebAXRoleSpinButton: result += " SPIN_BUTTON"; break;
    case WebKit::WebAXRoleSpinButtonPart: result += " SPIN_BUTTON_PART"; break;
    case WebKit::WebAXRoleSplitGroup: result += " SPLIT_GROUP"; break;
    case WebKit::WebAXRoleSplitter: result += " SPLITTER"; break;
    case WebKit::WebAXRoleStaticText: result += " STATIC_TEXT"; break;
    case WebKit::WebAXRoleStatus: result += " STATUS"; break;
    case WebKit::WebAXRoleSystemWide: result += " SYSTEM_WIDE"; break;
    case WebKit::WebAXRoleTab: result += " TAB"; break;
    case WebKit::WebAXRoleTabList: result += " TAB_LIST"; break;
    case WebKit::WebAXRoleTabPanel: result += " TAB_PANEL"; break;
    case WebKit::WebAXRoleTable: result += " TABLE"; break;
    case WebKit::WebAXRoleTableHeaderContainer:
        result += " TABLE_HDR_CONTAINER"; break;
    case WebKit::WebAXRoleTextArea: result += " TEXTAREA"; break;
    case WebKit::WebAXRoleTextField: result += " TEXT_FIELD"; break;
    case WebKit::WebAXRoleTimer: result += " TIMER"; break;
    case WebKit::WebAXRoleToggleButton: result += " TOGGLE_BUTTON"; break;
    case WebKit::WebAXRoleToolbar: result += " TOOLBAR"; break;
    case WebKit::WebAXRoleTree: result += " TREE"; break;
    case WebKit::WebAXRoleTreeGrid: result += " TREE_GRID"; break;
    case WebKit::WebAXRoleTreeItem: result += " TREE_ITEM"; break;
    case WebKit::WebAXRoleUnknown: result += " UNKNOWN"; break;
    case WebKit::WebAXRoleUserInterfaceTooltip: result += " TOOLTIP"; break;
    case WebKit::WebAXRoleValueIndicator: result += " VALUE_INDICATOR"; break;
    case WebKit::WebAXRoleWebArea: result += " WEB_AREA"; break;
    case WebKit::WebAXRoleWindow: result += " WINDOW"; break;
    default:
      assert(false);
  }

  if (state & (1 << WebKit::WebAXStateBusy))
    result += " BUSY";
  if (state & (1 << WebKit::WebAXStateChecked))
    result += " CHECKED";
  if (state & (1 << WebKit::WebAXStateCollapsed))
    result += " COLLAPSED";
  if (state & (1 << WebKit::WebAXStateExpanded))
    result += " EXPANDED";
  if (state & (1 << WebKit::WebAXStateFocusable))
    result += " FOCUSABLE";
  if (state & (1 << WebKit::WebAXStateFocused))
    result += " FOCUSED";
  if (state & (1 << WebKit::WebAXStateHaspopup))
    result += " HASPOPUP";
  if (state & (1 << WebKit::WebAXStateHovered))
    result += " HOTTRACKED";
  if (state & (1 << WebKit::WebAXStateIndeterminate))
    result += " INDETERMINATE";
  if (state & (1 << WebKit::WebAXStateInvisible))
    result += " INVISIBLE";
  if (state & (1 << WebKit::WebAXStateLinked))
    result += " LINKED";
  if (state & (1 << WebKit::WebAXStateMultiselectable))
    result += " MULTISELECTABLE";
  if (state & (1 << WebKit::WebAXStateOffscreen))
    result += " OFFSCREEN";
  if (state & (1 << WebKit::WebAXStatePressed))
    result += " PRESSED";
  if (state & (1 << WebKit::WebAXStateProtected))
    result += " PROTECTED";
  if (state & (1 << WebKit::WebAXStateReadonly))
    result += " READONLY";
  if (state & (1 << WebKit::WebAXStateRequired))
    result += " REQUIRED";
  if (state & (1 << WebKit::WebAXStateSelectable))
    result += " SELECTABLE";
  if (state & (1 << WebKit::WebAXStateSelected))
    result += " SELECTED";
  if (state & (1 << WebKit::WebAXStateVertical))
    result += " VERTICAL";
  if (state & (1 << WebKit::WebAXStateVisited))
    result += " VISITED";

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  for (size_t i = 0; i < int_attributes.size(); ++i) {
    std::string value = IntToString(int_attributes[i].second);
    switch (int_attributes[i].first) {
      case ATTR_SCROLL_X:
        result += " scroll_x=" + value;
        break;
      case ATTR_SCROLL_X_MIN:
        result += " scroll_x_min=" + value;
        break;
      case ATTR_SCROLL_X_MAX:
        result += " scroll_x_max=" + value;
        break;
      case ATTR_SCROLL_Y:
        result += " scroll_y=" + value;
        break;
      case ATTR_SCROLL_Y_MIN:
        result += " scroll_y_min=" + value;
        break;
      case ATTR_SCROLL_Y_MAX:
        result += " scroll_y_max=" + value;
        break;
      case ATTR_HIERARCHICAL_LEVEL:
        result += " level=" + value;
        break;
      case ATTR_TEXT_SEL_START:
        result += " sel_start=" + value;
        break;
      case ATTR_TEXT_SEL_END:
        result += " sel_end=" + value;
        break;
      case ATTR_TABLE_ROW_COUNT:
        result += " rows=" + value;
        break;
      case ATTR_TABLE_COLUMN_COUNT:
        result += " cols=" + value;
        break;
      case ATTR_TABLE_CELL_COLUMN_INDEX:
        result += " col=" + value;
        break;
      case ATTR_TABLE_CELL_ROW_INDEX:
        result += " row=" + value;
        break;
      case ATTR_TABLE_CELL_COLUMN_SPAN:
        result += " colspan=" + value;
        break;
      case ATTR_TABLE_CELL_ROW_SPAN:
        result += " rowspan=" + value;
        break;
      case ATTR_TABLE_COLUMN_HEADER_ID:
        result += " column_header_id=" + value;
        break;
      case ATTR_TABLE_COLUMN_INDEX:
        result += " column_index=" + value;
        break;
      case ATTR_TABLE_HEADER_ID:
        result += " header_id=" + value;
        break;
      case ATTR_TABLE_ROW_HEADER_ID:
        result += " row_header_id=" + value;
        break;
      case ATTR_TABLE_ROW_INDEX:
        result += " row_index=" + value;
        break;
      case ATTR_TITLE_UI_ELEMENT:
        result += " title_elem=" + value;
        break;
      case ATTR_COLOR_VALUE_RED:
        result += " color_value_red=" + value;
        break;
      case ATTR_COLOR_VALUE_GREEN:
        result += " color_value_green=" + value;
        break;
      case ATTR_COLOR_VALUE_BLUE:
        result += " color_value_blue=" + value;
        break;
      case ATTR_TEXT_DIRECTION:
        switch (int_attributes[i].second) {
          case WebKit::WebAXTextDirectionLR:
          default:
            result += " text_direction=lr";
            break;
          case WebKit::WebAXTextDirectionRL:
            result += " text_direction=rl";
            break;
          case WebKit::WebAXTextDirectionTB:
            result += " text_direction=tb";
            break;
          case WebKit::WebAXTextDirectionBT:
            result += " text_direction=bt";
            break;
        }
        break;
    }
  }

  for (size_t i = 0; i < string_attributes.size(); ++i) {
    std::string value = string_attributes[i].second;
    switch (string_attributes[i].first) {
      case ATTR_DOC_URL:
        result += " doc_url=" + value;
        break;
      case ATTR_DOC_TITLE:
        result += " doc_title=" + value;
        break;
      case ATTR_DOC_MIMETYPE:
        result += " doc_mimetype=" + value;
        break;
      case ATTR_DOC_DOCTYPE:
        result += " doc_doctype=" + value;
        break;
      case ATTR_ACCESS_KEY:
        result += " access_key=" + value;
        break;
      case ATTR_ACTION:
        result += " action=" + value;
        break;
      case ATTR_DESCRIPTION:
        result += " description=" + value;
        break;
      case ATTR_DISPLAY:
        result += " display=" + value;
        break;
      case ATTR_HELP:
        result += " help=" + value;
        break;
      case ATTR_HTML_TAG:
        result += " html_tag=" + value;
        break;
      case ATTR_LIVE_RELEVANT:
        result += " relevant=" + value;
        break;
      case ATTR_LIVE_STATUS:
        result += " live=" + value;
        break;
      case ATTR_CONTAINER_LIVE_RELEVANT:
        result += " container_relevant=" + value;
        break;
      case ATTR_CONTAINER_LIVE_STATUS:
        result += " container_live=" + value;
        break;
      case ATTR_ROLE:
        result += " role=" + value;
        break;
      case ATTR_SHORTCUT:
        result += " shortcut=" + value;
        break;
      case ATTR_URL:
        result += " url=" + value;
        break;
      case ATTR_NAME:
        result += " name=" + value;
        break;
      case ATTR_VALUE:
        result += " value=" + value;
        break;
    }
  }

  for (size_t i = 0; i < float_attributes.size(); ++i) {
    std::string value = DoubleToString(float_attributes[i].second);
    switch (float_attributes[i].first) {
      case ATTR_DOC_LOADING_PROGRESS:
        result += " doc_progress=" + value;
        break;
      case ATTR_VALUE_FOR_RANGE:
        result += " value_for_range=" + value;
        break;
      case ATTR_MAX_VALUE_FOR_RANGE:
        result += " max_value=" + value;
        break;
      case ATTR_MIN_VALUE_FOR_RANGE:
        result += " min_value=" + value;
        break;
    }
  }

  for (size_t i = 0; i < bool_attributes.size(); ++i) {
    std::string value = bool_attributes[i].second ? "true" : "false";
    switch (bool_attributes[i].first) {
      case ATTR_DOC_LOADED:
        result += " doc_loaded=" + value;
        break;
      case ATTR_BUTTON_MIXED:
        result += " mixed=" + value;
        break;
      case ATTR_LIVE_ATOMIC:
        result += " atomic=" + value;
        break;
      case ATTR_LIVE_BUSY:
        result += " busy=" + value;
        break;
      case ATTR_CONTAINER_LIVE_ATOMIC:
        result += " container_atomic=" + value;
        break;
      case ATTR_CONTAINER_LIVE_BUSY:
        result += " container_busy=" + value;
        break;
      case ATTR_ARIA_READONLY:
        result += " aria_readonly=" + value;
        break;
      case ATTR_CAN_SET_VALUE:
        result += " can_set_value=" + value;
        break;
      case ATTR_UPDATE_LOCATION_ONLY:
        result += " update_location_only=" + value;
        break;
      case ATTR_CANVAS_HAS_FALLBACK:
        result += " has_fallback=" + value;
        break;
    }
  }

  for (size_t i = 0; i < intlist_attributes.size(); ++i) {
    const std::vector<int32>& values = intlist_attributes[i].second;
    switch (intlist_attributes[i].first) {
      case ATTR_INDIRECT_CHILD_IDS:
        result += " indirect_child_ids=" + IntVectorToString(values);
        break;
      case ATTR_LINE_BREAKS:
        result += " line_breaks=" + IntVectorToString(values);
        break;
      case ATTR_CELL_IDS:
        result += " cell_ids=" + IntVectorToString(values);
        break;
      case ATTR_UNIQUE_CELL_IDS:
        result += " unique_cell_ids=" + IntVectorToString(values);
        break;
      case ATTR_CHARACTER_OFFSETS:
        result += " character_offsets=" + IntVectorToString(values);
        break;
      case ATTR_WORD_STARTS:
        result += " word_starts=" + IntVectorToString(values);
        break;
      case ATTR_WORD_ENDS:
        result += " word_ends=" + IntVectorToString(values);
        break;
    }
  }

  if (!child_ids.empty())
    result += " child_ids=" + IntVectorToString(child_ids);

  return result;
}

std::string AccessibilityNodeDataTreeNode::DebugString(bool recursive) const {
  std::string result;

  static int indent = 0;
  result += "\n";
  for (int i = 0; i < indent; ++i)
    result += "  ";

  result += AccessibilityNodeData::DebugString(recursive);

  if (recursive) {
    result += "\n";
    ++indent;
    for (size_t i = 0; i < children.size(); ++i)
      result += children[i].DebugString(true);
    --indent;
  }

  return result;
}

#endif  // ifndef NDEBUG

}  // namespace content
