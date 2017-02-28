// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

struct WebCompositionUnderline;

}  // namespace blink

namespace content {

class RenderFrameHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAndroid;

// This class is in charge of dispatching key events from the java side
// and forward to renderer along with input method results via
// corresponding host view.
// Ownership of these objects remains on the native side (see
// RenderWidgetHostViewAndroid).
class CONTENT_EXPORT ImeAdapterAndroid {
 public:
  explicit ImeAdapterAndroid(RenderWidgetHostViewAndroid* rwhva);
  ~ImeAdapterAndroid();

  // Called from java -> native
  bool SendKeyEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobject>& original_key_event,
      int type,
      int modifiers,
      long event_time,
      int key_code,
      int scan_code,
      bool is_system_key,
      int unicode_text);
  void SetComposingText(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& text,
                        const base::android::JavaParamRef<jstring>& text_str,
                        int relative_cursor_pos);
  void CommitText(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& text,
                  const base::android::JavaParamRef<jstring>& text_str,
                  int relative_cursor_pos);
  void FinishComposingText(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>&);
  void AttachImeAdapter(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>& java_object);
  void SetEditableSelectionOffsets(JNIEnv*,
                                   const base::android::JavaParamRef<jobject>&,
                                   int start,
                                   int end);
  void SetComposingRegion(JNIEnv*,
                          const base::android::JavaParamRef<jobject>&,
                          int start,
                          int end);
  void DeleteSurroundingText(JNIEnv*,
                             const base::android::JavaParamRef<jobject>&,
                             int before,
                             int after);
  void DeleteSurroundingTextInCodePoints(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      int before,
      int after);
  void ResetImeAdapter(JNIEnv*, const base::android::JavaParamRef<jobject>&);
  void RequestCursorUpdate(JNIEnv*, const base::android::JavaParamRef<jobject>&,
                           bool immediateRequest, bool monitorRequest);
  bool RequestTextInputStateUpdate(JNIEnv*,
                                   const base::android::JavaParamRef<jobject>&);

  // Called from native -> java
  void CancelComposition();
  void FocusedNodeChanged(bool is_editable_node);
  void SetCharacterBounds(const std::vector<gfx::RectF>& rects);

  base::android::ScopedJavaLocalRef<jobject> java_ime_adapter_for_testing(
      JNIEnv* env) {
    return java_ime_adapter_.get(env);
  }

 private:
  RenderWidgetHostImpl* GetFocusedWidget();
  RenderFrameHost* GetFocusedFrame();
  std::vector<blink::WebCompositionUnderline> GetUnderlinesFromSpans(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& text,
      const base::string16& text16);

  RenderWidgetHostViewAndroid* rwhva_;
  JavaObjectWeakGlobalRef java_ime_adapter_;
};

bool RegisterImeAdapter(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_
