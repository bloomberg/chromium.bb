// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ime_adapter_android.h"

#include <android/input.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "jni/ImeAdapter_jni.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;

namespace content {
namespace {

// Maps a java KeyEvent into a NativeWebKeyboardEvent.
// |java_key_event| is used to maintain a globalref for KeyEvent.
// |action| will help determine the WebInputEvent type.
// type, |modifiers|, |time_ms|, |key_code|, |unicode_char| is used to create
// WebKeyboardEvent. |key_code| is also needed ad need to treat the enter key
// as a key press of character \r.
NativeWebKeyboardEvent NativeWebKeyboardEventFromKeyEvent(
    JNIEnv* env,
    jobject java_key_event,
    int action,
    int modifiers,
    long time_ms,
    int key_code,
    bool is_system_key,
    int unicode_char) {
  WebKit::WebInputEvent::Type type = WebKit::WebInputEvent::Undefined;
  if (action == AKEY_EVENT_ACTION_DOWN)
    type = WebKit::WebInputEvent::RawKeyDown;
  else if (action == AKEY_EVENT_ACTION_UP)
    type = WebKit::WebInputEvent::KeyUp;
  return NativeWebKeyboardEvent(java_key_event, type, modifiers,
      time_ms, key_code, unicode_char, is_system_key);
}

}  // anonymous namespace

bool RegisterImeAdapter(JNIEnv* env) {
  if (!RegisterNativesImpl(env))
    return false;

  Java_ImeAdapter_initializeWebInputEvents(env,
                                           WebKit::WebInputEvent::RawKeyDown,
                                           WebKit::WebInputEvent::KeyUp,
                                           WebKit::WebInputEvent::Char,
                                           WebKit::WebInputEvent::ShiftKey,
                                           WebKit::WebInputEvent::AltKey,
                                           WebKit::WebInputEvent::ControlKey,
                                           WebKit::WebInputEvent::CapsLockOn,
                                           WebKit::WebInputEvent::NumLockOn);
  // TODO(miguelg): remove date time related enums after
  // https://bugs.webkit.org/show_bug.cgi?id=100935.
  Java_ImeAdapter_initializeTextInputTypes(
      env,
      ui::TEXT_INPUT_TYPE_NONE,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_TYPE_TEXT_AREA,
      ui::TEXT_INPUT_TYPE_PASSWORD,
      ui::TEXT_INPUT_TYPE_SEARCH,
      ui::TEXT_INPUT_TYPE_URL,
      ui::TEXT_INPUT_TYPE_EMAIL,
      ui::TEXT_INPUT_TYPE_TELEPHONE,
      ui::TEXT_INPUT_TYPE_NUMBER,
      ui::TEXT_INPUT_TYPE_DATE,
      ui::TEXT_INPUT_TYPE_DATE_TIME,
      ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
      ui::TEXT_INPUT_TYPE_MONTH,
      ui::TEXT_INPUT_TYPE_TIME,
      ui::TEXT_INPUT_TYPE_WEEK,
      ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
  return true;
}

ImeAdapterAndroid::ImeAdapterAndroid(RenderWidgetHostViewAndroid* rwhva)
    : rwhva_(rwhva),
      java_ime_adapter_(NULL) {
}

ImeAdapterAndroid::~ImeAdapterAndroid() {
  if (java_ime_adapter_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ImeAdapter_detach(env, java_ime_adapter_);
    env->DeleteGlobalRef(java_ime_adapter_);
  }
}

bool ImeAdapterAndroid::SendSyntheticKeyEvent(JNIEnv*,
                                              jobject,
                                              int type,
                                              long time_ms,
                                              int key_code,
                                              int text) {
  NativeWebKeyboardEvent event(static_cast<WebKit::WebInputEvent::Type>(type),
                               0 /* modifiers */, time_ms / 1000.0, key_code,
                               text, false /* is_system_key */);
  rwhva_->SendKeyEvent(event);
  return true;
}

bool ImeAdapterAndroid::SendKeyEvent(JNIEnv* env, jobject,
                                     jobject original_key_event,
                                     int action, int modifiers,
                                     long time_ms, int key_code,
                                     bool is_system_key, int unicode_char) {
  NativeWebKeyboardEvent event = NativeWebKeyboardEventFromKeyEvent(
          env, original_key_event, action, modifiers,
          time_ms, key_code, is_system_key, unicode_char);
  rwhva_->SendKeyEvent(event);
  if (event.type == WebKit::WebInputEvent::RawKeyDown && event.text[0]) {
    // Send a Char event, but without an os_event since we don't want to
    // roundtrip back to java such synthetic event.
    NativeWebKeyboardEvent char_event(event);
    char_event.os_event = NULL;
    event.type = WebKit::WebInputEvent::Char;
    rwhva_->SendKeyEvent(event);
  }
  return true;
}

void ImeAdapterAndroid::SetComposingText(JNIEnv* env, jobject, jstring text,
                                         int new_cursor_pos) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  string16 text16 = ConvertJavaStringToUTF16(env, text);
  std::vector<WebKit::WebCompositionUnderline> underlines;
  underlines.push_back(
      WebKit::WebCompositionUnderline(0, text16.length(), SK_ColorBLACK,
                                      false));
  // new_cursor_position is as described in the Android API for
  // InputConnection#setComposingText, whereas the parameters for
  // ImeSetComposition are relative to the start of the composition.
  if (new_cursor_pos > 0)
    new_cursor_pos = text16.length() + new_cursor_pos - 1;

  rwhi->ImeSetComposition(text16, underlines, new_cursor_pos, new_cursor_pos);
}

void ImeAdapterAndroid::ImeBatchStateChanged(JNIEnv* env,
                                             jobject,
                                             jboolean is_begin) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_ImeBatchStateChanged(rwhi->GetRoutingID(), is_begin));
}

void ImeAdapterAndroid::CommitText(JNIEnv* env, jobject, jstring text) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  string16 text16 = ConvertJavaStringToUTF16(env, text);
  rwhi->ImeConfirmComposition(text16);
}

void ImeAdapterAndroid::AttachImeAdapter(JNIEnv* env, jobject java_object) {
  java_ime_adapter_ = AttachCurrentThread()->NewGlobalRef(java_object);
}

void ImeAdapterAndroid::CancelComposition() {
  Java_ImeAdapter_cancelComposition(AttachCurrentThread(), java_ime_adapter_);
}

void ImeAdapterAndroid::SetEditableSelectionOffsets(JNIEnv*, jobject,
                                                    int start, int end) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_SetEditableSelectionOffsets(rwhi->GetRoutingID(),
                                                     start, end));
}

void ImeAdapterAndroid::SetComposingRegion(JNIEnv*, jobject,
                                           int start, int end) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  std::vector<WebKit::WebCompositionUnderline> underlines;
  underlines.push_back(
      WebKit::WebCompositionUnderline(0, end - start, SK_ColorBLACK, false));

  rwhi->Send(new ViewMsg_SetCompositionFromExistingText(
      rwhi->GetRoutingID(), start, end, underlines));
}

void ImeAdapterAndroid::DeleteSurroundingText(JNIEnv*, jobject,
                                              int before, int after) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_ExtendSelectionAndDelete(rwhi->GetRoutingID(),
                                                  before, after));
}

void ImeAdapterAndroid::Unselect(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Unselect(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::SelectAll(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_SelectAll(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Cut(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Cut(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Copy(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Copy(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Paste(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Paste(rwhi->GetRoutingID()));
}

}  // namespace content
