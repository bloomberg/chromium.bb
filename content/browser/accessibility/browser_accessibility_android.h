// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/browser/accessibility/browser_accessibility.h"

namespace content {

class BrowserAccessibilityAndroid : public BrowserAccessibility {
 public:
  // Overrides from BrowserAccessibility.
  virtual void PostInitialize() OVERRIDE;
  virtual bool IsNative() const OVERRIDE;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Actions
  void FocusJNI(JNIEnv* env, jobject obj);
  void ClickJNI(JNIEnv* env, jobject obj);

  // Const accessors
  jboolean GetClickableJNI(JNIEnv* env, jobject obj) const;
  jboolean IsFocusedJNI(JNIEnv* env, jobject obj) const;
  jboolean IsEditableTextJNI(JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetNameJNI(
      JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jobject> GetAbsoluteRectJNI(
      JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jobject> GetRectInParentJNI(
      JNIEnv* env, jobject obj) const;
  jboolean IsFocusableJNI(JNIEnv* env, jobject obj) const;
  jint GetParentJNI(JNIEnv* env, jobject obj) const;
  jint GetChildCountJNI(JNIEnv* env, jobject obj) const;
  jint GetChildIdAtJNI(
      JNIEnv* env, jobject obj, jint child_index) const;
  jboolean IsCheckableJNI(JNIEnv* env, jobject obj) const;
  jboolean IsCheckedJNI(JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetClassNameJNI(
      JNIEnv* env, jobject obj) const;
  jboolean IsEnabledJNI(JNIEnv* env, jobject obj) const;
  jboolean IsPasswordJNI(JNIEnv* env, jobject obj) const;
  jboolean IsScrollableJNI(JNIEnv* env, jobject obj) const;
  jboolean IsSelectedJNI(JNIEnv* env, jobject obj) const;
  jboolean IsVisibleJNI(JNIEnv* env, jobject obj) const;
  jint GetItemIndexJNI(JNIEnv* env, jobject obj) const;
  jint GetItemCountJNI(JNIEnv* env, jobject obj) const;
  jint GetScrollXJNI(JNIEnv* env, jobject obj) const;
  jint GetScrollYJNI(JNIEnv* env, jobject obj) const;
  jint GetMaxScrollXJNI(JNIEnv* env, jobject obj) const;
  jint GetMaxScrollYJNI(JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetAriaLiveJNI(
      JNIEnv* env, jobject obj) const;
  jint GetSelectionStartJNI(JNIEnv* env, jobject obj) const;
  jint GetSelectionEndJNI(JNIEnv* env, jobject obj) const;
  jint GetEditableTextLengthJNI(JNIEnv* env, jobject obj) const;
  jint GetTextChangeFromIndexJNI(JNIEnv* env, jobject obj) const;
  jint GetTextChangeAddedCountJNI(JNIEnv* env, jobject obj) const;
  jint GetTextChangeRemovedCountJNI(JNIEnv* env, jobject obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetTextChangeBeforeTextJNI(
      JNIEnv* env, jobject obj) const;

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  // Allow BrowserAccessibilityManagerAndroid to call these private methods.
  friend class BrowserAccessibilityManagerAndroid;

  BrowserAccessibilityAndroid();

  string16 ComputeName() const;
  string16 GetAriaLive() const;
  bool IsFocusable() const;
  bool HasFocusableChild() const;
  bool HasOnlyStaticTextChildren() const;
  bool IsIframe() const;
  bool IsLeaf() const;

  void NotifyLiveRegionUpdate(string16& aria_live);

  string16 cached_text_;
  bool first_time_;
  string16 old_value_;
  string16 new_value_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroid);
};

bool RegisterBrowserAccessibility(JNIEnv* env);

}  // namespace content

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
