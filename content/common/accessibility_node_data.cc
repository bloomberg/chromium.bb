// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/accessibility_node_data.h"

#include <set>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

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
      role(ROLE_UNKNOWN),
      state(-1) {
}

AccessibilityNodeData::~AccessibilityNodeData() {
}

#ifndef NDEBUG
std::string AccessibilityNodeData::DebugString(bool recursive) const {
  std::string result;
  static int indent = 0;
  result += "\n";
  for (int i = 0; i < indent; ++i)
    result += "  ";

  result += "id=" + IntToString(id);

  switch (role) {
    case ROLE_ALERT: result += " ALERT"; break;
    case ROLE_ALERT_DIALOG: result += " ALERT_DIALOG"; break;
    case ROLE_ANNOTATION: result += " ANNOTATION"; break;
    case ROLE_APPLICATION: result += " APPLICATION"; break;
    case ROLE_ARTICLE: result += " ARTICLE"; break;
    case ROLE_BROWSER: result += " BROWSER"; break;
    case ROLE_BUSY_INDICATOR: result += " BUSY_INDICATOR"; break;
    case ROLE_BUTTON: result += " BUTTON"; break;
    case ROLE_CANVAS: result += " CANVAS"; break;
    case ROLE_CANVAS_WITH_FALLBACK_CONTENT: result += " CANVAS_FALLBACK"; break;
    case ROLE_CELL: result += " CELL"; break;
    case ROLE_CHECKBOX: result += " CHECKBOX"; break;
    case ROLE_COLOR_WELL: result += " COLOR_WELL"; break;
    case ROLE_COLUMN: result += " COLUMN"; break;
    case ROLE_COLUMN_HEADER: result += " COLUMN_HEADER"; break;
    case ROLE_COMBO_BOX: result += " COMBO_BOX"; break;
    case ROLE_DEFINITION_LIST_DEFINITION: result += " DL_DEFINITION"; break;
    case ROLE_DEFINITION_LIST_TERM: result += " DL_TERM"; break;
    case ROLE_DIALOG: result += " DIALOG"; break;
    case ROLE_DIRECTORY: result += " DIRECTORY"; break;
    case ROLE_DISCLOSURE_TRIANGLE: result += " DISCLOSURE_TRIANGLE"; break;
    case ROLE_DIV: result += " DIV"; break;
    case ROLE_DOCUMENT: result += " DOCUMENT"; break;
    case ROLE_DRAWER: result += " DRAWER"; break;
    case ROLE_EDITABLE_TEXT: result += " EDITABLE_TEXT"; break;
    case ROLE_FOOTER: result += " FOOTER"; break;
    case ROLE_FORM: result += " FORM"; break;
    case ROLE_GRID: result += " GRID"; break;
    case ROLE_GROUP: result += " GROUP"; break;
    case ROLE_GROW_AREA: result += " GROW_AREA"; break;
    case ROLE_HEADING: result += " HEADING"; break;
    case ROLE_HELP_TAG: result += " HELP_TAG"; break;
    case ROLE_HORIZONTAL_RULE: result += " HORIZONTAL_RULE"; break;
    case ROLE_IGNORED: result += " IGNORED"; break;
    case ROLE_IMAGE: result += " IMAGE"; break;
    case ROLE_IMAGE_MAP: result += " IMAGE_MAP"; break;
    case ROLE_IMAGE_MAP_LINK: result += " IMAGE_MAP_LINK"; break;
    case ROLE_INCREMENTOR: result += " INCREMENTOR"; break;
    case ROLE_LABEL: result += " LABEL"; break;
    case ROLE_LANDMARK_APPLICATION: result += " L_APPLICATION"; break;
    case ROLE_LANDMARK_BANNER: result += " L_BANNER"; break;
    case ROLE_LANDMARK_COMPLEMENTARY: result += " L_COMPLEMENTARY"; break;
    case ROLE_LANDMARK_CONTENTINFO: result += " L_CONTENTINFO"; break;
    case ROLE_LANDMARK_MAIN: result += " L_MAIN"; break;
    case ROLE_LANDMARK_NAVIGATION: result += " L_NAVIGATION"; break;
    case ROLE_LANDMARK_SEARCH: result += " L_SEARCH"; break;
    case ROLE_LINK: result += " LINK"; break;
    case ROLE_LIST: result += " LIST"; break;
    case ROLE_LISTBOX: result += " LISTBOX"; break;
    case ROLE_LISTBOX_OPTION: result += " LISTBOX_OPTION"; break;
    case ROLE_LIST_ITEM: result += " LIST_ITEM"; break;
    case ROLE_LIST_MARKER: result += " LIST_MARKER"; break;
    case ROLE_LOG: result += " LOG"; break;
    case ROLE_MARQUEE: result += " MARQUEE"; break;
    case ROLE_MATH: result += " MATH"; break;
    case ROLE_MATTE: result += " MATTE"; break;
    case ROLE_MENU: result += " MENU"; break;
    case ROLE_MENU_BAR: result += " MENU_BAR"; break;
    case ROLE_MENU_BUTTON: result += " MENU_BUTTON"; break;
    case ROLE_MENU_ITEM: result += " MENU_ITEM"; break;
    case ROLE_MENU_LIST_OPTION: result += " MENU_LIST_OPTION"; break;
    case ROLE_MENU_LIST_POPUP: result += " MENU_LIST_POPUP"; break;
    case ROLE_NOTE: result += " NOTE"; break;
    case ROLE_OUTLINE: result += " OUTLINE"; break;
    case ROLE_PARAGRAPH: result += " PARAGRAPH"; break;
    case ROLE_POPUP_BUTTON: result += " POPUP_BUTTON"; break;
    case ROLE_PRESENTATIONAL: result += " PRESENTATIONAL"; break;
    case ROLE_PROGRESS_INDICATOR: result += " PROGRESS_INDICATOR"; break;
    case ROLE_RADIO_BUTTON: result += " RADIO_BUTTON"; break;
    case ROLE_RADIO_GROUP: result += " RADIO_GROUP"; break;
    case ROLE_REGION: result += " REGION"; break;
    case ROLE_ROOT_WEB_AREA: result += " ROOT_WEB_AREA"; break;
    case ROLE_ROW: result += " ROW"; break;
    case ROLE_ROW_HEADER: result += " ROW_HEADER"; break;
    case ROLE_RULER: result += " RULER"; break;
    case ROLE_RULER_MARKER: result += " RULER_MARKER"; break;
    case ROLE_SCROLLAREA: result += " SCROLLAREA"; break;
    case ROLE_SCROLLBAR: result += " SCROLLBAR"; break;
    case ROLE_SHEET: result += " SHEET"; break;
    case ROLE_SLIDER: result += " SLIDER"; break;
    case ROLE_SLIDER_THUMB: result += " SLIDER_THUMB"; break;
    case ROLE_SPIN_BUTTON: result += " SPIN_BUTTON"; break;
    case ROLE_SPIN_BUTTON_PART: result += " SPIN_BUTTON_PART"; break;
    case ROLE_SPLITTER: result += " SPLITTER"; break;
    case ROLE_SPLIT_GROUP: result += " SPLIT_GROUP"; break;
    case ROLE_STATIC_TEXT: result += " STATIC_TEXT"; break;
    case ROLE_STATUS: result += " STATUS"; break;
    case ROLE_SYSTEM_WIDE: result += " SYSTEM_WIDE"; break;
    case ROLE_TAB: result += " TAB"; break;
    case ROLE_TABLE: result += " TABLE"; break;
    case ROLE_TABLE_HEADER_CONTAINER: result += " TABLE_HDR_CONTAINER"; break;
    case ROLE_TAB_GROUP_UNUSED: result += " TAB_GROUP_UNUSED"; break;
    case ROLE_TAB_LIST: result += " TAB_LIST"; break;
    case ROLE_TAB_PANEL: result += " TAB_PANEL"; break;
    case ROLE_TEXTAREA: result += " TEXTAREA"; break;
    case ROLE_TEXT_FIELD: result += " TEXT_FIELD"; break;
    case ROLE_TIMER: result += " TIMER"; break;
    case ROLE_TOGGLE_BUTTON: result += " TOGGLE_BUTTON"; break;
    case ROLE_TOOLBAR: result += " TOOLBAR"; break;
    case ROLE_TOOLTIP: result += " TOOLTIP"; break;
    case ROLE_TREE: result += " TREE"; break;
    case ROLE_TREE_GRID: result += " TREE_GRID"; break;
    case ROLE_TREE_ITEM: result += " TREE_ITEM"; break;
    case ROLE_UNKNOWN: result += " UNKNOWN"; break;
    case ROLE_VALUE_INDICATOR: result += " VALUE_INDICATOR"; break;
    case ROLE_WEBCORE_LINK: result += " WEBCORE_LINK"; break;
    case ROLE_WEB_AREA: result += " WEB_AREA"; break;
    case ROLE_WINDOW: result += " WINDOW"; break;
    default:
      assert(false);
  }

  if (state & (1 << STATE_BUSY))
    result += " BUSY";
  if (state & (1 << STATE_CHECKED))
    result += " CHECKED";
  if (state & (1 << STATE_COLLAPSED))
    result += " COLLAPSED";
  if (state & (1 << STATE_EXPANDED))
    result += " EXPANDED";
  if (state & (1 << STATE_FOCUSABLE))
    result += " FOCUSABLE";
  if (state & (1 << STATE_FOCUSED))
    result += " FOCUSED";
  if (state & (1 << STATE_HASPOPUP))
    result += " HASPOPUP";
  if (state & (1 << STATE_HOTTRACKED))
    result += " HOTTRACKED";
  if (state & (1 << STATE_INDETERMINATE))
    result += " INDETERMINATE";
  if (state & (1 << STATE_INVISIBLE))
    result += " INVISIBLE";
  if (state & (1 << STATE_LINKED))
    result += " LINKED";
  if (state & (1 << STATE_MULTISELECTABLE))
    result += " MULTISELECTABLE";
  if (state & (1 << STATE_OFFSCREEN))
    result += " OFFSCREEN";
  if (state & (1 << STATE_PRESSED))
    result += " PRESSED";
  if (state & (1 << STATE_PROTECTED))
    result += " PROTECTED";
  if (state & (1 << STATE_READONLY))
    result += " READONLY";
  if (state & (1 << STATE_REQUIRED))
    result += " REQUIRED";
  if (state & (1 << STATE_SELECTABLE))
    result += " SELECTABLE";
  if (state & (1 << STATE_SELECTED))
    result += " SELECTED";
  if (state & (1 << STATE_TRAVERSED))
    result += " TRAVERSED";
  if (state & (1 << STATE_UNAVAILABLE))
    result += " UNAVAILABLE";
  if (state & (1 << STATE_VERTICAL))
    result += " VERTICAL";
  if (state & (1 << STATE_VISITED))
    result += " VISITED";

  std::string tmp = UTF16ToUTF8(name);
  RemoveChars(tmp, "\n", &tmp);
  if (!tmp.empty())
    result += " name=" + tmp;

  tmp = UTF16ToUTF8(value);
  RemoveChars(tmp, "\n", &tmp);
  if (!tmp.empty())
    result += " value=" + tmp;

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  for (std::map<IntAttribute, int32>::const_iterator iter =
           int_attributes.begin();
       iter != int_attributes.end();
       ++iter) {
    std::string value = IntToString(iter->second);
    switch (iter->first) {
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
    case ATTR_TITLE_UI_ELEMENT:
        result += " title_elem=" + value;
        break;
    }
  }

  for (std::map<StringAttribute, string16>::const_iterator iter =
           string_attributes.begin();
       iter != string_attributes.end();
       ++iter) {
    std::string value = UTF16ToUTF8(iter->second);
    switch (iter->first) {
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
    }
  }

  for (std::map<FloatAttribute, float>::const_iterator iter =
           float_attributes.begin();
       iter != float_attributes.end();
       ++iter) {
    std::string value = DoubleToString(iter->second);
    switch (iter->first) {
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

  for (std::map<BoolAttribute, bool>::const_iterator iter =
           bool_attributes.begin();
       iter != bool_attributes.end();
       ++iter) {
    std::string value = iter->second ? "true" : "false";
    switch (iter->first) {
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
    }
  }

  if (!children.empty())
    result += " children=" + IntToString(children.size());

  if (!indirect_child_ids.empty())
    result += " indirect_child_ids=" + IntVectorToString(indirect_child_ids);

  if (!line_breaks.empty())
    result += " line_breaks=" + IntVectorToString(line_breaks);

  if (!cell_ids.empty())
    result += " cell_ids=" + IntVectorToString(cell_ids);

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
