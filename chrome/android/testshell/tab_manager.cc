// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/testshell/tab_manager.h"

#include "base/logging.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/browser/android/tab_base_android_impl.h"
#include "content/public/browser/android/content_view_layer_renderer.h"
#include "jni/TabManager_jni.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"

#include <android/native_window_jni.h>

using base::android::ScopedJavaLocalRef;
using content::ContentViewLayerRenderer;

namespace chrome {

// Register native methods
bool RegisterTabManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void ShowTab(JNIEnv* env, jclass clazz, jint jtab,
                    jint j_content_view_layer_renderer) {
  TabBaseAndroidImpl* tab = reinterpret_cast<TabBaseAndroidImpl*>(jtab);
  ContentViewLayerRenderer* content_view_layer_renderer =
      reinterpret_cast<ContentViewLayerRenderer*>(
          j_content_view_layer_renderer);
  content_view_layer_renderer->AttachLayer(tab->tab_layer());
}

static void HideTab(JNIEnv* env, jclass clazz, jint jtab,
                    jint j_content_view_layer_renderer) {
  TabBaseAndroidImpl* tab = reinterpret_cast<TabBaseAndroidImpl*>(jtab);
  ContentViewLayerRenderer* content_view_layer_renderer =
      reinterpret_cast<ContentViewLayerRenderer*>(
          j_content_view_layer_renderer);
  content_view_layer_renderer->DetachLayer(tab->tab_layer());
}

}  // namespace chrome
