// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/public/common/input_event_ack_state.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

class WebContents;

// Native class for GestureListenerManagerImpl.
class GestureListenerManager : public RenderWidgetHostConnector {
 public:
  GestureListenerManager(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         WebContents* web_contents);
  ~GestureListenerManager() override;

  void Reset(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result);
  void DidStopFlinging();

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

 private:
  // A weak reference to the Java GestureListenerManager object.
  JavaObjectWeakGlobalRef java_ref_;

  DISALLOW_COPY_AND_ASSIGN(GestureListenerManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
