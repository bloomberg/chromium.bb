// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#include <oleacc.h>

#include <string>

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/public/test/accessibility_test_utils_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/iaccessible2/ia2_api_all.h"


string16 DumpAccessibilityTreeHelper::ToString(
    BrowserAccessibility* node, char* prefix) {
  BrowserAccessibilityWin* acc_obj = node->ToBrowserAccessibilityWin();

  // Get state string.
  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;
  VARIANT msaa_state_variant;
  HRESULT hresult = acc_obj->get_accState(variant_self, &msaa_state_variant);
  EXPECT_EQ(S_OK, hresult);
  EXPECT_EQ(VT_I4, msaa_state_variant.vt);
  string16 state_str = IAccessibleStateToString(msaa_state_variant.lVal);
  string16 state2_str = IAccessible2StateToString(acc_obj->ia2_state());
  if (!state2_str.empty()) {
    if (!state_str.empty())
      state_str += L",";
    state_str += state2_str;
  }

  // Get role string.
  string16 role_str = IAccessible2RoleToString(acc_obj->ia2_role());

  return UTF8ToUTF16(prefix) +
         role_str +
         L" name='" + acc_obj->name() +
         L"' state=" + state_str + L"\n";
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetActualFileSuffix()
    const {
  return FILE_PATH_LITERAL("-actual-win.txt");
}

const FilePath::StringType DumpAccessibilityTreeHelper::GetExpectedFileSuffix()
    const {
  return FILE_PATH_LITERAL("-expected-win.txt");
}
