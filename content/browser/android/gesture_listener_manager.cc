// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/GestureListenerManagerImpl_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/android/gesture_event_type.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

GestureListenerManager::GestureListenerManager(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               WebContents* web_contents)
    : java_ref_(env, obj) {
}

GestureListenerManager::~GestureListenerManager() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onDestroy(env, j_obj);
}

void GestureListenerManager::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  // TODO(jinsukkim): Define WebInputEvent in Java.
  switch (event.GetType()) {
    case WebInputEvent::kGestureFlingStart:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED) {
        // The view expects the fling velocity in pixels/s.
        Java_GestureListenerManagerImpl_onFlingStartEventConsumed(env, j_obj);
      } else {
        // If a scroll ends with a fling, a SCROLL_END event is never sent.
        // However, if that fling went unconsumed, we still need to let the
        // listeners know that scrolling has ended.
        Java_GestureListenerManagerImpl_onScrollEndEventAck(env, j_obj);
      }
      break;
    case WebInputEvent::kGestureScrollBegin:
      Java_GestureListenerManagerImpl_onScrollBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::kGestureScrollUpdate:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_GestureListenerManagerImpl_onScrollUpdateGestureConsumed(env,
                                                                      j_obj);
      break;
    case WebInputEvent::kGestureScrollEnd:
      Java_GestureListenerManagerImpl_onScrollEndEventAck(env, j_obj);
      break;
    case WebInputEvent::kGesturePinchBegin:
      Java_GestureListenerManagerImpl_onPinchBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::kGesturePinchEnd:
      Java_GestureListenerManagerImpl_onPinchEndEventAck(env, j_obj);
      break;
    case WebInputEvent::kGestureTap:
      Java_GestureListenerManagerImpl_onSingleTapEventAck(
          env, j_obj, ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
      break;
    case WebInputEvent::kGestureLongPress:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_GestureListenerManagerImpl_onLongPressAck(env, j_obj);
      break;
    default:
      break;
  }
}

void JNI_GestureListenerManagerImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents) << "Should be created with a valid WebContents.";

  WebContentsViewAndroid* view = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents)->GetView());
  view->SetGestureListenerManager(
      base::MakeUnique<GestureListenerManager>(env, obj, web_contents));
}

}  // namespace content
