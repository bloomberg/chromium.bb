// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/android/content_view_core_impl.h"

namespace content {

namespace aria_strings {
  extern const char kAriaLivePolite[];
  extern const char kAriaLiveAssertive[];
}

// From android.view.accessibility.AccessibilityNodeInfo in Java:
enum AndroidMovementGranularity {
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER = 1,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD = 2,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE = 4
};

// From android.view.accessibility.AccessibilityEvent in Java:
enum {
  ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED = 16,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED = 8192,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 131072
};

class BrowserAccessibilityAndroid;

class CONTENT_EXPORT BrowserAccessibilityManagerAndroid
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAndroid(
      base::android::ScopedJavaLocalRef<jobject> content_view_core,
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerAndroid() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  void SetContentViewCore(
      base::android::ScopedJavaLocalRef<jobject> content_view_core);

  // Implementation of BrowserAccessibilityManager.
  void NotifyAccessibilityEvent(ui::AXEvent event_type,
                                BrowserAccessibility* node) override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Tree methods.
  jint GetRootId(JNIEnv* env, jobject obj);
  jboolean IsNodeValid(JNIEnv* env, jobject obj, jint id);
  void HitTest(JNIEnv* env, jobject obj, jint x, jint y);

  // Methods to get information about a specific node.
  jboolean IsEditableText(JNIEnv* env, jobject obj, jint id);
  jint GetEditableTextSelectionStart(JNIEnv* env, jobject obj, jint id);
  jint GetEditableTextSelectionEnd(JNIEnv* env, jobject obj, jint id);

  // Populate Java accessibility data structures with info about a node.
  jboolean PopulateAccessibilityNodeInfo(
      JNIEnv* env, jobject obj, jobject info, jint id);
  jboolean PopulateAccessibilityEvent(
      JNIEnv* env, jobject obj, jobject event, jint id, jint event_type);

  // Perform actions.
  void Click(JNIEnv* env, jobject obj, jint id);
  void Focus(JNIEnv* env, jobject obj, jint id);
  void Blur(JNIEnv* env, jobject obj);
  void ScrollToMakeNodeVisible(JNIEnv* env, jobject obj, jint id);
  void SetTextFieldValue(JNIEnv* env, jobject obj, jint id, jstring value);
  void SetSelection(JNIEnv* env, jobject obj, jint id, jint start, jint end);
  jboolean AdjustSlider(JNIEnv* env, jobject obj, jint id, jboolean increment);

  // Return the id of the next node in tree order in the direction given by
  // |forwards|, starting with |start_id|, that matches |element_type|,
  // where |element_type| is a special uppercase string from TalkBack or
  // BrailleBack indicating general categories of web content like
  // "SECTION" or "CONTROL".  Return 0 if not found.
  jint FindElementType(JNIEnv* env, jobject obj, jint start_id,
                       jstring element_type, jboolean forwards);

  // Respond to a ACTION_[NEXT/PREVIOUS]_AT_MOVEMENT_GRANULARITY action
  // and move the cursor/selection within the given node id. We keep track
  // of our own selection in BrowserAccessibilityManager.java for static
  // text, but if this is an editable text node, updates the selected text
  // in Blink, too, and either way calls
  // Java_BrowserAccessibilityManager_finishGranularityMove with the
  // result.
  jboolean NextAtGranularity(JNIEnv* env, jobject obj,
                             jint granularity, jboolean extend_selection,
                             jint id, jint cursor_index);
  jboolean PreviousAtGranularity(JNIEnv* env, jobject obj,
                                 jint granularity, jboolean extend_selection,
                                 jint id, jint cursor_index);

  // Helper functions to compute the next start and end index when moving
  // forwards or backwards by character, word, or line. This part is
  // unit-tested; the Java interfaces above are just wrappers. Both of these
  // take a single cursor index as input and return the boundaries surrounding
  // the next word or line. If moving by character, the output start and
  // end index will be the same.
  bool NextAtGranularity(
      int32 granularity, int cursor_index,
      BrowserAccessibilityAndroid* node, int32* start_index, int32* end_index);
  bool PreviousAtGranularity(
      int32 granularity, int cursor_index,
      BrowserAccessibilityAndroid* node, int32* start_index, int32* end_index);

  // Set accessibility focus. This sends a message to the renderer to
  // asynchronously load inline text boxes for this node only, enabling more
  // accurate movement by granularities on this node.
  void SetAccessibilityFocus(JNIEnv* env, jobject obj, jint id);

 protected:
  // AXTreeDelegate overrides.
  void OnAtomicUpdateFinished(
      bool root_changed,
      const std::vector<ui::AXTreeDelegate::Change>& changes) override;

  bool UseRootScrollOffsetsWhenComputingBounds() override;

 private:
  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  // A weak reference to the Java BrowserAccessibilityManager object.
  // This avoids adding another reference to BrowserAccessibilityManager and
  // preventing garbage collection.
  // Premature garbage collection is prevented by the long-lived reference in
  // ContentViewCore.
  JavaObjectWeakGlobalRef java_ref_;

  // Handle a hover event from the renderer process.
  void HandleHoverEvent(BrowserAccessibility* node);

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerAndroid);
};

bool RegisterBrowserAccessibilityManager(JNIEnv* env);

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
