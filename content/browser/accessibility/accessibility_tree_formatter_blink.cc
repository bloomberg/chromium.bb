// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace content {

AccessibilityTreeFormatterBlink::AccessibilityTreeFormatterBlink()
    : AccessibilityTreeFormatterBrowser() {}

AccessibilityTreeFormatterBlink::~AccessibilityTreeFormatterBlink() {
}

const char* const TREE_DATA_ATTRIBUTES[] = {"TreeData.textSelStartOffset",
                                            "TreeData.textSelEndOffset"};

const char* STATE_FOCUSED = "focused";
const char* STATE_OFFSCREEN = "offscreen";

uint32_t AccessibilityTreeFormatterBlink::ChildCount(
    const BrowserAccessibility& node) const {
  if (node.HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    return node.PlatformChildCount();
  else
    return node.InternalChildCount();
}

BrowserAccessibility* AccessibilityTreeFormatterBlink::GetChild(
    const BrowserAccessibility& node,
    uint32_t i) const {
  if (node.HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID))
    return node.PlatformGetChild(i);
  else
    return node.InternalGetChild(i);
}

// TODO(aleventhal) Convert ax enums to friendly strings, e.g. AXCheckedState.
std::string AccessibilityTreeFormatterBlink::IntAttrToString(
    const BrowserAccessibility& node,
    ui::AXIntAttribute attr,
    int value) const {
  if (ui::IsNodeIdIntAttribute(attr)) {
    // Relation
    BrowserAccessibility* target = node.manager()->GetFromID(value);
    return target ? ui::ToString(target->GetData().role) : std::string("null");
  }

  switch (attr) {
    case ui::AX_ATTR_ARIA_CURRENT_STATE:
      return ui::ToString(static_cast<ui::AXAriaCurrentState>(value));
    case ui::AX_ATTR_CHECKED_STATE:
      return ui::ToString(static_cast<ui::AXCheckedState>(value));
    case ui::AX_ATTR_DEFAULT_ACTION_VERB:
      return ui::ToString(static_cast<ui::AXDefaultActionVerb>(value));
    case ui::AX_ATTR_DESCRIPTION_FROM:
      return ui::ToString(static_cast<ui::AXDescriptionFrom>(value));
    case ui::AX_ATTR_INVALID_STATE:
      return ui::ToString(static_cast<ui::AXInvalidState>(value));
    case ui::AX_ATTR_NAME_FROM:
      return ui::ToString(static_cast<ui::AXNameFrom>(value));
    case ui::AX_ATTR_RESTRICTION:
      return ui::ToString(static_cast<ui::AXRestriction>(value));
    case ui::AX_ATTR_SORT_DIRECTION:
      return ui::ToString(static_cast<ui::AXSortDirection>(value));
    case ui::AX_ATTR_TEXT_DIRECTION:
      return ui::ToString(static_cast<ui::AXTextDirection>(value));
    // No pretty printing necessary for these:
    case ui::AX_ATTR_ACTIVEDESCENDANT_ID:
    case ui::AX_ATTR_ARIA_CELL_COLUMN_INDEX:
    case ui::AX_ATTR_ARIA_CELL_ROW_INDEX:
    case ui::AX_ATTR_ARIA_COLUMN_COUNT:
    case ui::AX_ATTR_ARIA_ROW_COUNT:
    case ui::AX_ATTR_BACKGROUND_COLOR:
    case ui::AX_ATTR_CHILD_TREE_ID:
    case ui::AX_ATTR_COLOR:
    case ui::AX_ATTR_COLOR_VALUE:
    case ui::AX_ATTR_DETAILS_ID:
    case ui::AX_ATTR_ERRORMESSAGE_ID:
    case ui::AX_ATTR_HIERARCHICAL_LEVEL:
    case ui::AX_ATTR_IN_PAGE_LINK_TARGET_ID:
    case ui::AX_ATTR_MEMBER_OF_ID:
    case ui::AX_ATTR_NEXT_ON_LINE_ID:
    case ui::AX_ATTR_POS_IN_SET:
    case ui::AX_ATTR_PREVIOUS_ON_LINE_ID:
    case ui::AX_ATTR_SCROLL_X:
    case ui::AX_ATTR_SCROLL_X_MAX:
    case ui::AX_ATTR_SCROLL_X_MIN:
    case ui::AX_ATTR_SCROLL_Y:
    case ui::AX_ATTR_SCROLL_Y_MAX:
    case ui::AX_ATTR_SCROLL_Y_MIN:
    case ui::AX_ATTR_SET_SIZE:
    case ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX:
    case ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN:
    case ui::AX_ATTR_TABLE_CELL_ROW_INDEX:
    case ui::AX_ATTR_TABLE_CELL_ROW_SPAN:
    case ui::AX_ATTR_TABLE_COLUMN_COUNT:
    case ui::AX_ATTR_TABLE_COLUMN_HEADER_ID:
    case ui::AX_ATTR_TABLE_COLUMN_INDEX:
    case ui::AX_ATTR_TABLE_HEADER_ID:
    case ui::AX_ATTR_TABLE_ROW_COUNT:
    case ui::AX_ATTR_TABLE_ROW_HEADER_ID:
    case ui::AX_ATTR_TABLE_ROW_INDEX:
    case ui::AX_ATTR_TEXT_SEL_END:
    case ui::AX_ATTR_TEXT_SEL_START:
    case ui::AX_ATTR_TEXT_STYLE:
    case ui::AX_INT_ATTRIBUTE_NONE:
      break;
  }

  // Just return the number
  return std::to_string(value);
}

void AccessibilityTreeFormatterBlink::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  int id = node.GetId();
  dict->SetInteger("id", id);

  dict->SetString("internalRole", ui::ToString(node.GetData().role));

  gfx::Rect bounds = gfx::ToEnclosingRect(node.GetData().location);
  dict->SetInteger("boundsX", bounds.x());
  dict->SetInteger("boundsY", bounds.y());
  dict->SetInteger("boundsWidth", bounds.width());
  dict->SetInteger("boundsHeight", bounds.height());

  bool offscreen = false;
  gfx::Rect page_bounds = node.GetPageBoundsRect(&offscreen);
  dict->SetInteger("pageBoundsX", page_bounds.x());
  dict->SetInteger("pageBoundsY", page_bounds.y());
  dict->SetInteger("pageBoundsWidth", page_bounds.width());
  dict->SetInteger("pageBoundsHeight", page_bounds.height());

  dict->SetBoolean("transform",
                   node.GetData().transform &&
                   !node.GetData().transform->IsIdentity());

  for (int state_index = ui::AX_STATE_NONE;
       state_index <= ui::AX_STATE_LAST;
       ++state_index) {
    auto state = static_cast<ui::AXState>(state_index);
    if (node.HasState(state))
      dict->SetBoolean(ui::ToString(state), true);
  }

  if (offscreen)
    dict->SetBoolean(STATE_OFFSCREEN, true);

  for (int attr_index = ui::AX_STRING_ATTRIBUTE_NONE;
       attr_index <= ui::AX_STRING_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXStringAttribute>(attr_index);
    if (node.HasStringAttribute(attr))
      dict->SetString(ui::ToString(attr), node.GetStringAttribute(attr));
  }

  for (int attr_index = ui::AX_INT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntAttribute>(attr_index);
    if (node.HasIntAttribute(attr)) {
      int value = node.GetIntAttribute(attr);
      dict->SetString(ui::ToString(attr), IntAttrToString(node, attr, value));
    }
  }

  for (int attr_index = ui::AX_FLOAT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_FLOAT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXFloatAttribute>(attr_index);
    if (node.HasFloatAttribute(attr))
      dict->SetDouble(ui::ToString(attr), node.GetFloatAttribute(attr));
  }

  for (int attr_index = ui::AX_BOOL_ATTRIBUTE_NONE;
       attr_index <= ui::AX_BOOL_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXBoolAttribute>(attr_index);
    if (node.HasBoolAttribute(attr))
      dict->SetBoolean(ui::ToString(attr), node.GetBoolAttribute(attr));
  }

  for (int attr_index = ui::AX_INT_LIST_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_LIST_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntListAttribute>(attr_index);
    if (node.HasIntListAttribute(attr)) {
      std::vector<int32_t> values;
      node.GetIntListAttribute(attr, &values);
      auto value_list = base::MakeUnique<base::ListValue>();
      for (size_t i = 0; i < values.size(); ++i) {
        if (ui::IsNodeIdIntListAttribute(attr)) {
          BrowserAccessibility* target = node.manager()->GetFromID(values[i]);
          if (target)
            value_list->AppendString(ui::ToString(target->GetData().role));
          else
            value_list->AppendString("null");
        } else {
          value_list->AppendInteger(values[i]);
        }
      }
      dict->Set(ui::ToString(attr), std::move(value_list));
    }
  }

  //  Check for relevant rich text selection info in AXTreeData
  int anchor_id = node.manager()->GetTreeData().sel_anchor_object_id;
  if (id == anchor_id) {
    int anchor_offset = node.manager()->GetTreeData().sel_anchor_offset;
    dict->SetInteger("TreeData.textSelStartOffset", anchor_offset);
  }
  int focus_id = node.manager()->GetTreeData().sel_focus_object_id;
  if (id == focus_id) {
    int focus_offset = node.manager()->GetTreeData().sel_focus_offset;
    dict->SetInteger("TreeData.textSelEndOffset", focus_offset);
  }

  std::vector<std::string> actions_strings;
  for (int action_index = ui::AX_ACTION_NONE + 1;
       action_index <= ui::AX_ACTION_LAST; ++action_index) {
    auto action = static_cast<ui::AXAction>(action_index);
    if (node.HasAction(action))
      actions_strings.push_back(ui::ToString(action));
  }
  if (!actions_strings.empty())
    dict->SetString("actions", base::JoinString(actions_strings, ","));
}

base::string16 AccessibilityTreeFormatterBlink::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
  base::string16 error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  base::string16 line;

  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
    WriteAttribute(true, base::IntToString16(id_value), &line);
  }

  base::string16 role_value;
  dict.GetString("internalRole", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  for (int state_index = ui::AX_STATE_NONE;
       state_index <= ui::AX_STATE_LAST;
       ++state_index) {
    auto state = static_cast<ui::AXState>(state_index);
    const base::Value* value;
    if (!dict.Get(ui::ToString(state), &value))
      continue;

    WriteAttribute(false, ui::ToString(state), &line);
  }

  // Offscreen and Focused states are not in the state list.
  bool value = false;
  dict.GetBoolean(STATE_OFFSCREEN, &value);
  if (value)
    WriteAttribute(false, STATE_OFFSCREEN, &line);
  dict.GetBoolean(STATE_FOCUSED, &value);
  if (value)
    WriteAttribute(false, STATE_FOCUSED, &line);

  WriteAttribute(false,
                 FormatCoordinates("location", "boundsX", "boundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("size", "boundsWidth", "boundsHeight", dict),
                 &line);

  WriteAttribute(false,
                 FormatCoordinates("pageLocation",
                                   "pageBoundsX", "pageBoundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("pageSize",
                                   "pageBoundsWidth", "pageBoundsHeight", dict),
                 &line);

  bool transform;
  if (dict.GetBoolean("transform", &transform) && transform)
    WriteAttribute(false, "transform", &line);

  for (int attr_index = ui::AX_STRING_ATTRIBUTE_NONE;
       attr_index <= ui::AX_STRING_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXStringAttribute>(attr_index);
    std::string string_value;
    if (!dict.GetString(ui::ToString(attr), &string_value))
      continue;
    WriteAttribute(
        false,
        base::StringPrintf("%s='%s'", ui::ToString(attr), string_value.c_str()),
        &line);
  }

  for (int attr_index = ui::AX_INT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntAttribute>(attr_index);
    std::string string_value;
    if (!dict.GetString(ui::ToString(attr), &string_value))
      continue;
    WriteAttribute(
        false,
        base::StringPrintf("%s=%s", ui::ToString(attr), string_value.c_str()),
        &line);
  }

  for (int attr_index = ui::AX_BOOL_ATTRIBUTE_NONE;
       attr_index <= ui::AX_BOOL_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXBoolAttribute>(attr_index);
    bool bool_value;
    if (!dict.GetBoolean(ui::ToString(attr), &bool_value))
      continue;
    WriteAttribute(false,
                   base::StringPrintf("%s=%s", ui::ToString(attr),
                                      bool_value ? "true" : "false"),
                   &line);
  }

  for (int attr_index = ui::AX_FLOAT_ATTRIBUTE_NONE;
       attr_index <= ui::AX_FLOAT_ATTRIBUTE_LAST; ++attr_index) {
    auto attr = static_cast<ui::AXFloatAttribute>(attr_index);
    double float_value;
    if (!dict.GetDouble(ui::ToString(attr), &float_value))
      continue;
    WriteAttribute(
        false, base::StringPrintf("%s=%.2f", ui::ToString(attr), float_value),
        &line);
  }

  for (int attr_index = ui::AX_INT_LIST_ATTRIBUTE_NONE;
       attr_index <= ui::AX_INT_LIST_ATTRIBUTE_LAST;
       ++attr_index) {
    auto attr = static_cast<ui::AXIntListAttribute>(attr_index);
    const base::ListValue* value;
    if (!dict.GetList(ui::ToString(attr), &value))
      continue;
    std::string attr_string(ui::ToString(attr));
    attr_string.push_back('=');
    for (size_t i = 0; i < value->GetSize(); ++i) {
      if (i > 0)
        attr_string += ",";
      if (ui::IsNodeIdIntListAttribute(attr)) {
        std::string string_value;
        value->GetString(i, &string_value);
        attr_string += string_value;
      } else {
        int int_value;
        value->GetInteger(i, &int_value);
        attr_string += base::IntToString(int_value);
      }
    }
    WriteAttribute(false, attr_string, &line);
  }

  std::string string_value;
  if (dict.GetString("actions", &string_value)) {
    WriteAttribute(false,
                   base::StringPrintf("%s=%s", "actions", string_value.c_str()),
                   &line);
  }

  for (const char* attribute_name : TREE_DATA_ATTRIBUTES) {
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->type()) {
      case base::Value::Type::STRING: {
        std::string string_value;
        value->GetAsString(&string_value);
        WriteAttribute(
            false,
            base::StringPrintf("%s=%s", attribute_name, string_value.c_str()),
            &line);
        break;
      }
      case base::Value::Type::INTEGER: {
        int int_value = 0;
        value->GetAsInteger(&int_value);
        WriteAttribute(false,
                       base::StringPrintf("%s=%d", attribute_name, int_value),
                       &line);
        break;
      }
      case base::Value::Type::DOUBLE: {
        double double_value = 0.0;
        value->GetAsDouble(&double_value);
        WriteAttribute(
            false, base::StringPrintf("%s=%.2f", attribute_name, double_value),
            &line);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  return line;
}

const base::FilePath::StringType
AccessibilityTreeFormatterBlink::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-blink.txt");
}

const std::string AccessibilityTreeFormatterBlink::GetAllowEmptyString() {
  return "@BLINK-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterBlink::GetAllowString() {
  return "@BLINK-ALLOW:";
}

const std::string AccessibilityTreeFormatterBlink::GetDenyString() {
  return "@BLINK-DENY:";
}

}  // namespace content
