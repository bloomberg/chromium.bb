// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#include <oleacc.h>

#include <string>

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
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

  // Get state strings.
  std::vector<string16> state_strings;
  IAccessibleStateToStringVector(acc_obj->ia_state(), &state_strings);
  IAccessible2StateToStringVector(acc_obj->ia2_state(), &state_strings);

  // Get the description, help, and attributes.
  string16 description;
  acc_obj->GetStringAttribute(AccessibilityNodeData::ATTR_DESCRIPTION,
                              &description);
  string16 help;
  acc_obj->GetStringAttribute(AccessibilityNodeData::ATTR_HELP, &help);
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
  if (acc_obj->get_currentValue(&currentValue) != S_FALSE)
    Add(false, StringPrintf(L"currentValue=%.2f", V_R8(&currentValue)));
  VARIANT minimumValue;
  if (acc_obj->get_minimumValue(&minimumValue) != S_FALSE)
    Add(false, StringPrintf(L"minimumValue=%.2f", V_R8(&minimumValue)));
  VARIANT maximumValue;
  if (acc_obj->get_maximumValue(&maximumValue) != S_FALSE)
    Add(false, StringPrintf(L"maximumValue=%.2f", V_R8(&maximumValue)));
  Add(false, L"description='" + description + L"'");
  return UTF8ToUTF16(prefix) + FinishLine() + ASCIIToUTF16("\n");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetActualFileSuffix()
    const {
  return FILE_PATH_LITERAL("-actual-win.txt");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetExpectedFileSuffix()
    const {
  return FILE_PATH_LITERAL("-expected-win.txt");
}


const std::string DumpAccessibilityTreeHelper::GetAllowString() const {
  return "@WIN-ALLOW:";
}

const std::string DumpAccessibilityTreeHelper::GetDenyString() const {
  return "@WIN-DENY:";
}

}  // namespace content
