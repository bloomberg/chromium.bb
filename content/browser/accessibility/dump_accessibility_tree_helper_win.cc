// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#include <oleacc.h>

#include <string>

#include "base/files/file_path.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/accessibility_node_data.h"
#include "content/public/test/accessibility_test_utils_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"

namespace content {

void DumpAccessibilityTreeHelper::Initialize() {
  ui::win::CreateATLModuleIfNeeded();
}

string16 DumpAccessibilityTreeHelper::ToString(
    BrowserAccessibility* node, char* prefix) {
  BrowserAccessibilityWin* acc_obj = node->ToBrowserAccessibilityWin();

  // Get the computed name.
  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;
  CComBSTR msaa_variant;
  HRESULT hresult = acc_obj->get_accName(variant_self, &msaa_variant);
  string16 name;
  if (S_OK == hresult)
    name = msaa_variant.m_str;

  hresult = acc_obj->get_accValue(variant_self, &msaa_variant);
  string16 value;
  if (S_OK == hresult)
    value = msaa_variant.m_str;

  hresult = acc_obj->get_accDescription(variant_self, &msaa_variant);
  string16 description;
  if (S_OK == hresult)
    description = msaa_variant.m_str;

  hresult = acc_obj->get_accHelp(variant_self, &msaa_variant);
  string16 help;
  if (S_OK == hresult)
    help = msaa_variant.m_str;

  hresult = acc_obj->get_accDefaultAction(variant_self, &msaa_variant);
  string16 default_action;
  if (S_OK == hresult)
    default_action = msaa_variant.m_str;

  hresult = acc_obj->get_accKeyboardShortcut(variant_self, &msaa_variant);
  string16 keyboard_shortcut;
  if (S_OK == hresult)
    keyboard_shortcut = msaa_variant.m_str;

  // Get state strings.
  std::vector<string16> state_strings;
  IAccessibleStateToStringVector(acc_obj->ia_state(), &state_strings);
  IAccessible2StateToStringVector(acc_obj->ia2_state(), &state_strings);

  // Get the attributes.
  const std::vector<string16>& ia2_attributes = acc_obj->ia2_attributes();

  // Build the line.
  StartLine();
  Add(true, IAccessible2RoleToString(acc_obj->ia2_role()));
  Add(true, L"name='" + name + L"'");
  Add(false, L"value='" + value + L"'");
  for (std::vector<string16>::const_iterator it = state_strings.begin();
       it != state_strings.end();
       ++it) {
    Add(false, *it);
  }
  for (std::vector<string16>::const_iterator it = ia2_attributes.begin();
       it != ia2_attributes.end();
       ++it) {
    Add(false, *it);
  }
  Add(false, L"role_name='" + acc_obj->role_name() + L"'");
  VARIANT currentValue;
  if (acc_obj->get_currentValue(&currentValue) == S_OK)
    Add(false, StringPrintf(L"currentValue=%.2f", V_R8(&currentValue)));
  VARIANT minimumValue;
  if (acc_obj->get_minimumValue(&minimumValue) == S_OK)
    Add(false, StringPrintf(L"minimumValue=%.2f", V_R8(&minimumValue)));
  VARIANT maximumValue;
  if (acc_obj->get_maximumValue(&maximumValue) == S_OK)
    Add(false, StringPrintf(L"maximumValue=%.2f", V_R8(&maximumValue)));
  Add(false, L"description='" + description + L"'");
  Add(false, L"default_action='" + default_action + L"'");
  Add(false, L"keyboard_shortcut='" + keyboard_shortcut + L"'");
  BrowserAccessibility* root = node->manager()->GetRoot();
  LONG left, top, width, height;
  LONG root_left, root_top, root_width, root_height;
  if (S_FALSE != acc_obj->accLocation(
          &left, &top, &width, &height, variant_self) &&
      S_FALSE != root->ToBrowserAccessibilityWin()->accLocation(
          &root_left, &root_top, &root_width, &root_height, variant_self)) {
    Add(false, StringPrintf(L"location=(%d, %d)",
            left - root_left, top - root_top));
    Add(false, StringPrintf(L"size=(%d, %d)", width, height));
  }
  LONG index_in_parent;
  if (acc_obj->get_indexInParent(&index_in_parent) == S_OK)
    Add(false, StringPrintf(L"index_in_parent=%d", index_in_parent));
  LONG n_relations;
  if (acc_obj->get_nRelations(&n_relations) == S_OK)
    Add(false, StringPrintf(L"n_relations=%d", n_relations));
  LONG group_level, similar_items_in_group, position_in_group;
  if (acc_obj->get_groupPosition(&group_level,
                                 &similar_items_in_group,
                                 &position_in_group) == S_OK) {
    Add(false, StringPrintf(L"group_level=%d", group_level));
    Add(false, StringPrintf(L"similar_items_in_group=%d",
                            similar_items_in_group));
    Add(false, StringPrintf(L"position_in_group=%d", position_in_group));
  }
  LONG table_rows;
  if (acc_obj->get_nRows(&table_rows) == S_OK)
    Add(false, StringPrintf(L"table_rows=%d", table_rows));
  LONG table_columns;
  if (acc_obj->get_nRows(&table_columns) == S_OK)
    Add(false, StringPrintf(L"table_columns=%d", table_columns));
  LONG row_index;
  if (acc_obj->get_rowIndex(&row_index) == S_OK)
    Add(false, StringPrintf(L"row_index=%d", row_index));
  LONG column_index;
  if (acc_obj->get_columnIndex(&column_index) == S_OK)
    Add(false, StringPrintf(L"column_index=%d", column_index));
  LONG n_characters;
  if (acc_obj->get_nCharacters(&n_characters) == S_OK)
    Add(false, StringPrintf(L"n_characters=%d", n_characters));
  LONG caret_offset;
  if (acc_obj->get_caretOffset(&caret_offset) == S_OK)
    Add(false, StringPrintf(L"caret_offset=%d", caret_offset));
  LONG n_selections;
  if (acc_obj->get_nSelections(&n_selections) == S_OK) {
    Add(false, StringPrintf(L"n_selections=%d", n_selections));
    if (n_selections > 0) {
      LONG start, end;
      if (acc_obj->get_selection(0, &start, &end) == S_OK) {
        Add(false, StringPrintf(L"selection_start=%d", start));
        Add(false, StringPrintf(L"selection_end=%d", end));
      }
    }
  }

  return UTF8ToUTF16(prefix) + FinishLine() + ASCIIToUTF16("\n");
}

const base::FilePath::StringType
DumpAccessibilityTreeHelper::GetActualFileSuffix() const {
  return FILE_PATH_LITERAL("-actual-win.txt");
}

const base::FilePath::StringType
DumpAccessibilityTreeHelper::GetExpectedFileSuffix() const {
  return FILE_PATH_LITERAL("-expected-win.txt");
}

const std::string DumpAccessibilityTreeHelper::GetAllowEmptyString() const {
  return "@WIN-ALLOW-EMPTY:";
}

const std::string DumpAccessibilityTreeHelper::GetAllowString() const {
  return "@WIN-ALLOW:";
}

const std::string DumpAccessibilityTreeHelper::GetDenyString() const {
  return "@WIN-DENY:";
}

}  // namespace content
