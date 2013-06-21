// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/common/accessibility_node_data.h"

using base::StringPrintf;

namespace content {

namespace {
const char* BOOL_ATTRIBUTES[] = {
  "checkable",
  "checked",
  "clickable",
  "disabled",
  "editable_text",
  "focusable",
  "focused",
  "invisible",
  "password",
  "scrollable",
  "selected"
};

const char* STRING_ATTRIBUTES[] = {
  "name"
};

const char* INT_ATTRIBUTES[] = {
  "item_index",
  "item_count"
};
}

void AccessibilityTreeFormatter::Initialize() {
}

void AccessibilityTreeFormatter::AddProperties(
    const BrowserAccessibility& node, DictionaryValue* dict) {
  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);
  JNIEnv* env = base::android::AttachCurrentThread();

  // Class name.
  dict->SetString("class", base::android::ConvertJavaStringToUTF8(
      android_node->GetClassNameJNI(env, NULL)));

  // Bool attributes.
  dict->SetBoolean("focusable",
                   android_node->IsFocusableJNI(env, NULL));
  dict->SetBoolean("focused",
                   android_node->IsFocusedJNI(env, NULL));
  dict->SetBoolean("clickable",
                   android_node->GetClickableJNI(env, NULL));
  dict->SetBoolean("editable_text",
                   android_node->IsEditableTextJNI(env, NULL));
  dict->SetBoolean("checkable",
                   android_node->IsCheckableJNI(env, NULL));
  dict->SetBoolean("checked",
                   android_node->IsCheckedJNI(env, NULL));
  dict->SetBoolean("disabled",
                   !android_node->IsEnabledJNI(env, NULL));
  dict->SetBoolean("scrollable",
                   android_node->IsScrollableJNI(env, NULL));
  dict->SetBoolean("password",
                   android_node->IsPasswordJNI(env, NULL));
  dict->SetBoolean("selected",
                   android_node->IsSelectedJNI(env, NULL));
  dict->SetBoolean("invisible",
                   !android_node->IsVisibleJNI(env, NULL));

  // String attributes.
  dict->SetString("name", base::android::ConvertJavaStringToUTF8(
      android_node->GetNameJNI(env, NULL)));

  // Int attributes.
  dict->SetInteger("item_index",
                   android_node->GetItemIndexJNI(env, NULL));
  dict->SetInteger("item_count",
                   android_node->GetItemCountJNI(env, NULL));
}

bool AccessibilityTreeFormatter::IncludeChildren(
    const BrowserAccessibility& node) {
  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);
  JNIEnv* env = base::android::AttachCurrentThread();

  return 0 != android_node->GetChildCountJNI(env, NULL);
}

string16 AccessibilityTreeFormatter::ToString(const DictionaryValue& dict,
                                              const string16& indent) {
  string16 line;

  string16 class_value;
  dict.GetString("class", &class_value);
  WriteAttribute(true, UTF16ToUTF8(class_value), &line);

  for (unsigned i = 0; i < arraysize(BOOL_ATTRIBUTES); i++) {
    const char* attribute_name = BOOL_ATTRIBUTES[i];
    bool value;
    if (dict.GetBoolean(attribute_name, &value) && value)
      WriteAttribute(true, attribute_name, &line);
  }

  for (unsigned i = 0; i < arraysize(STRING_ATTRIBUTES); i++) {
    const char* attribute_name = STRING_ATTRIBUTES[i];
    std::string value;
    if (!dict.GetString(attribute_name, &value) || value.empty())
      continue;
    WriteAttribute(true,
                   StringPrintf("%s='%s'", attribute_name, value.c_str()),
                   &line);
  }

  for (unsigned i = 0; i < arraysize(INT_ATTRIBUTES); i++) {
    const char* attribute_name = INT_ATTRIBUTES[i];
    int value;
    if (!dict.GetInteger(attribute_name, &value) || value == 0)
      continue;
    WriteAttribute(true,
                   StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  return indent + line + ASCIIToUTF16("\n");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("-actual-android.txt");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-android.txt");
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "@ANDROID-ALLOW-EMPTY:";
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return "@ANDROID-ALLOW:";
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return "@ANDROID-DENY:";
}

}  // namespace content
