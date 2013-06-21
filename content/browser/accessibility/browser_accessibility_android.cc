// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/common/accessibility_messages.h"
#include "content/common/accessibility_node_data.h"

using base::android::ScopedJavaLocalRef;

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAndroid();
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid() {
  first_time_ = true;
}

bool BrowserAccessibilityAndroid::IsNative() const {
  return true;
}

//
// Actions, called from Java.
//

void BrowserAccessibilityAndroid::FocusJNI(JNIEnv* env, jobject obj) {
  manager_->SetFocus(this, true);
}

void BrowserAccessibilityAndroid::ClickJNI(JNIEnv* env, jobject obj) {
  manager_->DoDefaultAction(*this);
}

//
// Const accessors, called from Java.
//

ScopedJavaLocalRef<jstring>
BrowserAccessibilityAndroid::GetNameJNI(JNIEnv* env, jobject obj) const {
  return base::android::ConvertUTF16ToJavaString(env, ComputeName());
}

ScopedJavaLocalRef<jobject>
BrowserAccessibilityAndroid::GetAbsoluteRectJNI(
    JNIEnv* env, jobject obj) const {
  gfx::Rect rect = GetLocalBoundsRect();

  // TODO(aboxhall): replace with non-stub implementation
  return ScopedJavaLocalRef<jobject>(env, NULL);
}

ScopedJavaLocalRef<jobject>
BrowserAccessibilityAndroid::GetRectInParentJNI(
    JNIEnv* env, jobject obj) const {
  gfx::Rect rect = GetLocalBoundsRect();
  if (parent()) {
    gfx::Rect parent_rect = parent()->GetLocalBoundsRect();
    rect.Offset(-parent_rect.OffsetFromOrigin());
  }

  // TODO(aboxhall): replace with non-stub implementation
  return ScopedJavaLocalRef<jobject>(env, NULL);
}

jboolean
BrowserAccessibilityAndroid::IsFocusableJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(IsFocusable());
}

jboolean
BrowserAccessibilityAndroid::IsEditableTextJNI(JNIEnv* env, jobject obj) const {
  return IsEditableText();
}

jint BrowserAccessibilityAndroid::GetParentJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jint>(parent()->renderer_id());
}

jint
BrowserAccessibilityAndroid::GetChildCountJNI(JNIEnv* env, jobject obj) const {
  if (IsLeaf())
    return 0;
  else
    return static_cast<jint>(child_count());
}

jint BrowserAccessibilityAndroid::GetChildIdAtJNI(JNIEnv* env,
                                               jobject obj,
                                               jint child_index) const {
  return static_cast<jint>(GetChild(child_index)->renderer_id());
}

jboolean
BrowserAccessibilityAndroid::IsCheckableJNI(JNIEnv* env, jobject obj) const {
  bool checkable = false;
  bool is_aria_pressed_defined;
  bool is_mixed;
  GetAriaTristate("aria-pressed", &is_aria_pressed_defined, &is_mixed);
  if (role() == AccessibilityNodeData::ROLE_CHECKBOX ||
      role() == AccessibilityNodeData::ROLE_RADIO_BUTTON ||
      is_aria_pressed_defined) {
    checkable = true;
  }
  if (HasState(AccessibilityNodeData::STATE_CHECKED))
    checkable = true;
  return static_cast<jboolean>(checkable);
}

jboolean
BrowserAccessibilityAndroid::IsCheckedJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(
      HasState(AccessibilityNodeData::STATE_CHECKED));
}

base::android::ScopedJavaLocalRef<jstring>
BrowserAccessibilityAndroid::GetClassNameJNI(JNIEnv* env, jobject obj) const {
  const char* class_name = NULL;

  switch(role()) {
    case AccessibilityNodeData::ROLE_EDITABLE_TEXT:
    case AccessibilityNodeData::ROLE_SPIN_BUTTON:
    case AccessibilityNodeData::ROLE_TEXTAREA:
    case AccessibilityNodeData::ROLE_TEXT_FIELD:
      class_name = "android.widget.EditText";
      break;
    case AccessibilityNodeData::ROLE_SLIDER:
      class_name = "android.widget.SeekBar";
      break;
    case AccessibilityNodeData::ROLE_COMBO_BOX:
      class_name = "android.widget.Spinner";
      break;
    case AccessibilityNodeData::ROLE_BUTTON:
    case AccessibilityNodeData::ROLE_MENU_BUTTON:
    case AccessibilityNodeData::ROLE_POPUP_BUTTON:
      class_name = "android.widget.Button";
      break;
    case AccessibilityNodeData::ROLE_CHECKBOX:
      class_name = "android.widget.CheckBox";
      break;
    case AccessibilityNodeData::ROLE_RADIO_BUTTON:
      class_name = "android.widget.RadioButton";
      break;
    case AccessibilityNodeData::ROLE_TOGGLE_BUTTON:
      class_name = "android.widget.ToggleButton";
      break;
    case AccessibilityNodeData::ROLE_CANVAS:
    case AccessibilityNodeData::ROLE_IMAGE:
      class_name = "android.widget.Image";
      break;
    case AccessibilityNodeData::ROLE_PROGRESS_INDICATOR:
      class_name = "android.widget.ProgressBar";
      break;
    case AccessibilityNodeData::ROLE_TAB_LIST:
      class_name = "android.widget.TabWidget";
      break;
    case AccessibilityNodeData::ROLE_GRID:
    case AccessibilityNodeData::ROLE_TABLE:
      class_name = "android.widget.GridView";
      break;
    case AccessibilityNodeData::ROLE_LIST:
    case AccessibilityNodeData::ROLE_LISTBOX:
      class_name = "android.widget.ListView";
      break;
    default:
      class_name = "android.view.View";
      break;
  }

  return base::android::ConvertUTF8ToJavaString(env, class_name);
}

jboolean
BrowserAccessibilityAndroid::IsEnabledJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(
      !HasState(AccessibilityNodeData::STATE_UNAVAILABLE));
}

jboolean
BrowserAccessibilityAndroid::IsFocusedJNI(JNIEnv* env, jobject obj) const {
  return manager()->GetFocus(manager()->GetRoot()) == this;
}

jboolean
BrowserAccessibilityAndroid::IsPasswordJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(
      HasState(AccessibilityNodeData::STATE_PROTECTED));
}

jboolean
BrowserAccessibilityAndroid::IsScrollableJNI(JNIEnv* env, jobject obj) const {
  int dummy;
  bool scrollable = GetIntAttribute(
      AccessibilityNodeData::ATTR_SCROLL_X_MAX, &dummy);
  return static_cast<jboolean>(scrollable);
}

jboolean
BrowserAccessibilityAndroid::IsSelectedJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(
      HasState(AccessibilityNodeData::STATE_SELECTED));
}

jboolean
BrowserAccessibilityAndroid::IsVisibleJNI(JNIEnv* env, jobject obj) const {
  return static_cast<jboolean>(
      !HasState(AccessibilityNodeData::STATE_INVISIBLE));
}

base::android::ScopedJavaLocalRef<jstring>
BrowserAccessibilityAndroid::GetAriaLiveJNI(JNIEnv* env, jobject obj) const {
  return base::android::ConvertUTF16ToJavaString(env, GetAriaLive());
}

jint BrowserAccessibilityAndroid::GetItemIndexJNI(
    JNIEnv* env, jobject obj) const {
  int index = 0;
  switch(role()) {
    case AccessibilityNodeData::ROLE_LIST_ITEM:
    case AccessibilityNodeData::ROLE_LISTBOX_OPTION:
      index = index_in_parent();
      break;
    case AccessibilityNodeData::ROLE_SLIDER:
    case AccessibilityNodeData::ROLE_PROGRESS_INDICATOR: {
      float value_for_range;
      if (GetFloatAttribute(
              AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &value_for_range)) {
        index = static_cast<int>(value_for_range);
      }
      break;
    }
  }
  return static_cast<jint>(index);
}

jint
BrowserAccessibilityAndroid::GetItemCountJNI(JNIEnv* env, jobject obj) const {
  int count = 0;
  switch(role()) {
    case AccessibilityNodeData::ROLE_LIST:
    case AccessibilityNodeData::ROLE_LISTBOX:
      count = child_count();
      break;
    case AccessibilityNodeData::ROLE_SLIDER:
    case AccessibilityNodeData::ROLE_PROGRESS_INDICATOR: {
      float max_value_for_range;
      if (GetFloatAttribute(AccessibilityNodeData::ATTR_MAX_VALUE_FOR_RANGE,
                            &max_value_for_range)) {
        count = static_cast<int>(max_value_for_range);
      }
      break;
    }
  }
  return static_cast<jint>(count);
}

jint
BrowserAccessibilityAndroid::GetScrollXJNI(JNIEnv* env, jobject obj) const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X, &value);
  return static_cast<jint>(value);
}

jint
BrowserAccessibilityAndroid::GetScrollYJNI(JNIEnv* env, jobject obj) const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y, &value);
  return static_cast<jint>(value);
}

jint
BrowserAccessibilityAndroid::GetMaxScrollXJNI(JNIEnv* env, jobject obj) const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X_MAX, &value);
  return static_cast<jint>(value);
}

jint
BrowserAccessibilityAndroid::GetMaxScrollYJNI(JNIEnv* env, jobject obj) const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y_MAX, &value);
  return static_cast<jint>(value);
}

jboolean
BrowserAccessibilityAndroid::GetClickableJNI(JNIEnv* env, jobject obj) const {
  return (IsLeaf() && !ComputeName().empty());
}

jint BrowserAccessibilityAndroid::GetSelectionStartJNI(JNIEnv* env, jobject obj)
    const {
  int sel_start = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_START, &sel_start);
  return sel_start;
}

jint BrowserAccessibilityAndroid::GetSelectionEndJNI(JNIEnv* env, jobject obj)
    const {
  int sel_end = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &sel_end);
  return sel_end;
}

jint BrowserAccessibilityAndroid::GetEditableTextLengthJNI(
    JNIEnv* env, jobject obj) const {
  return value().length();
}

int BrowserAccessibilityAndroid::GetTextChangeFromIndexJNI(
    JNIEnv* env, jobject obj) const {
  size_t index = 0;
  while (index < old_value_.length() &&
         index < new_value_.length() &&
         old_value_[index] == new_value_[index]) {
    index++;
  }
  return index;
}

jint BrowserAccessibilityAndroid::GetTextChangeAddedCountJNI(
    JNIEnv* env, jobject obj) const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (new_len - left - right);
}

jint BrowserAccessibilityAndroid::GetTextChangeRemovedCountJNI(
    JNIEnv* env, jobject obj) const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (old_len - left - right);
}

base::android::ScopedJavaLocalRef<jstring>
BrowserAccessibilityAndroid::GetTextChangeBeforeTextJNI(
    JNIEnv* env, jobject obj) const {
  return base::android::ConvertUTF16ToJavaString(env, old_value_);
}

string16 BrowserAccessibilityAndroid::ComputeName() const {
  if (IsIframe() ||
      role() == AccessibilityNodeData::ROLE_WEB_AREA) {
    return string16();
  }

  string16 description;
  GetStringAttribute(AccessibilityNodeData::ATTR_DESCRIPTION,
                           &description);

  string16 text;
  if (!name().empty())
    text = name();
  else if (!description.empty())
    text = description;
  else if (!value().empty())
    text = value();

  if (text.empty() && HasOnlyStaticTextChildren()) {
    for (uint32 i = 0; i < child_count(); i++) {
      BrowserAccessibility* child = GetChild(i);
      text += static_cast<BrowserAccessibilityAndroid*>(child)->ComputeName();
    }
  }

  switch(role()) {
    case AccessibilityNodeData::ROLE_IMAGE_MAP_LINK:
    case AccessibilityNodeData::ROLE_LINK:
    case AccessibilityNodeData::ROLE_WEBCORE_LINK:
      if (!text.empty())
        text += ASCIIToUTF16(" ");
      text += ASCIIToUTF16("Link");
      break;
    case AccessibilityNodeData::ROLE_HEADING:
      // Only append "heading" if this node already has text.
      if (!text.empty())
        text += ASCIIToUTF16(" Heading");
      break;
  }

  return text;
}

string16 BrowserAccessibilityAndroid::GetAriaLive() const {
  string16 aria_live;
  if (GetStringAttribute(AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS,
                         &aria_live)) {
    return aria_live;
  }
  return string16();
}

bool BrowserAccessibilityAndroid::HasFocusableChild() const {
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = GetChild(i);
    if (child->HasState(AccessibilityNodeData::STATE_FOCUSABLE))
      return true;
    if (static_cast<BrowserAccessibilityAndroid*>(child)->HasFocusableChild())
      return true;
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasOnlyStaticTextChildren() const {
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = GetChild(i);
    if (child->role() != AccessibilityNodeData::ROLE_STATIC_TEXT)
      return false;
  }
  return true;
}

bool BrowserAccessibilityAndroid::IsIframe() const {
  string16 html_tag;
  GetStringAttribute(AccessibilityNodeData::ATTR_HTML_TAG, &html_tag);
  return html_tag == ASCIIToUTF16("iframe");
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  bool focusable = HasState(AccessibilityNodeData::STATE_FOCUSABLE);
  if (IsIframe() ||
      role() == AccessibilityNodeData::ROLE_WEB_AREA) {
    focusable = false;
  }
  return focusable;
}

bool BrowserAccessibilityAndroid::IsLeaf() const {
  if (child_count() == 0)
    return true;

  // Iframes are always allowed to contain children.
  if (IsIframe() ||
      role() == AccessibilityNodeData::ROLE_ROOT_WEB_AREA ||
      role() == AccessibilityNodeData::ROLE_WEB_AREA) {
    return false;
  }

  // If it has a focusable child, we definitely can't leave out children.
  if (HasFocusableChild())
    return false;

  // Headings with text can drop their children.
  string16 name = ComputeName();
  if (role() == AccessibilityNodeData::ROLE_HEADING && !name.empty())
    return true;

  // Focusable nodes with text can drop their children.
  if (HasState(AccessibilityNodeData::STATE_FOCUSABLE) && !name.empty())
    return true;

  // Nodes with only static text as children can drop their children.
  if (HasOnlyStaticTextChildren())
    return true;

  return false;
}

void BrowserAccessibilityAndroid::PostInitialize() {
  BrowserAccessibility::PostInitialize();

  if (IsEditableText()) {
    if (value_ != new_value_) {
      old_value_ = new_value_;
      new_value_ = value_;
    }
  }

  if (role_ == AccessibilityNodeData::ROLE_ALERT && first_time_)
    manager_->NotifyAccessibilityEvent(AccessibilityNotificationAlert, this);

  string16 live;
  if (GetStringAttribute(AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS,
                         &live)) {
    NotifyLiveRegionUpdate(live);
  }

  first_time_ = false;
}

void BrowserAccessibilityAndroid::NotifyLiveRegionUpdate(string16& aria_live) {
  if (!EqualsASCII(aria_live, aria_strings::kAriaLivePolite) &&
      !EqualsASCII(aria_live, aria_strings::kAriaLiveAssertive))
    return;

  string16 text = ComputeName();
  if (cached_text_ != text) {
    if (!text.empty()) {
      manager_->NotifyAccessibilityEvent(AccessibilityNotificationObjectShow,
                                         this);
    }
    cached_text_ = text;
  }
}

bool RegisterBrowserAccessibility(JNIEnv* env) {
  // TODO(aboxhall): replace with non-stub implementation
  return false;
}

}  // namespace content
