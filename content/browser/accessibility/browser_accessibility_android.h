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

  bool IsLeaf() const;

  bool IsCheckable() const;
  bool IsChecked() const;
  bool IsClickable() const;
  bool IsEnabled() const;
  bool IsFocusable() const;
  bool IsFocused() const;
  bool IsPassword() const;
  bool IsScrollable() const;
  bool IsSelected() const;
  bool IsVisibleToUser() const;

  const char* GetClassName() const;
  string16 GetText() const;

  int GetItemIndex() const;
  int GetItemCount() const;

  int GetScrollX() const;
  int GetScrollY() const;
  int GetMaxScrollX() const;
  int GetMaxScrollY() const;

  int GetTextChangeFromIndex() const;
  int GetTextChangeAddedCount() const;
  int GetTextChangeRemovedCount() const;
  string16 GetTextChangeBeforeText() const;

  int GetSelectionStart() const;
  int GetSelectionEnd() const;
  int GetEditableTextLength() const;

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  BrowserAccessibilityAndroid();

  bool HasFocusableChild() const;
  bool HasOnlyStaticTextChildren() const;
  bool IsIframe() const;

  void NotifyLiveRegionUpdate(string16& aria_live);

  string16 cached_text_;
  bool first_time_;
  string16 old_value_;
  string16 new_value_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroid);
};

}  // namespace content

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
