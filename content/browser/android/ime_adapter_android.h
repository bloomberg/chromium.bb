// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
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
class CONTENT_EXPORT ImeAdapterAndroid : public WebContentsObserver {
 public:
  ImeAdapterAndroid(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    WebContents* web_contents);
  ~ImeAdapterAndroid() override;

  // Called from java -> native
  bool SendKeyEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobject>& original_key_event,
      int type,
      int modifiers,
      jlong time_ms,
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
  void RequestCursorUpdate(JNIEnv*,
                           const base::android::JavaParamRef<jobject>&,
                           bool immediateRequest,
                           bool monitorRequest);
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

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void WebContentsDestroyed() override;

 private:
  RenderWidgetHostImpl* GetFocusedWidget();
  RenderFrameHost* GetFocusedFrame();
  std::vector<blink::WebCompositionUnderline> GetUnderlinesFromSpans(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& text,
      const base::string16& text16);
  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;
  void UpdateRenderProcessConnection(RenderWidgetHostViewAndroid* new_rwhva);

  base::WeakPtr<RenderWidgetHostViewAndroid> rwhva_;
  JavaObjectWeakGlobalRef java_ime_adapter_;
};

bool RegisterImeAdapter(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
