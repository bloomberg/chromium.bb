// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_renderer_priority_manager.h"

#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "jni/AwRendererPriorityManager_jni.h"

namespace android_webview {

AwRendererPriorityManager::AwRendererPriorityManager(
    content::RenderProcessHost* host)
    : host_(host), renderer_priority_(RENDERER_PRIORITY_INITIAL) {}

void AwRendererPriorityManager::SetRendererPriority(
    RendererPriority renderer_priority) {
  if (host_->GetHandle() == 0) {
    return;
  }
  if (renderer_priority_ != renderer_priority) {
    renderer_priority_ = renderer_priority;
    content::BrowserThread::PostTask(
        content::BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(&SetRendererPriorityOnLauncherThread, host_->GetHandle(),
                   renderer_priority_));
  }
}

// static
void AwRendererPriorityManager::SetRendererPriorityOnLauncherThread(
    int pid,
    RendererPriority renderer_priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_AwRendererPriorityManager_setRendererPriority(
      env, static_cast<jint>(pid), static_cast<jint>(renderer_priority));
}

}  // namespace android_webview
