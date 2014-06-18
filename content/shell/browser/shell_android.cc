// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "content/public/common/content_switches.h"
#include "content/shell/android/shell_manager.h"
#include "jni/Shell_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;

namespace content {

void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
}

void Shell::PlatformExit() {
}

void Shell::PlatformCleanUp() {
  JNIEnv* env = AttachCurrentThread();
  if (java_object_.is_null())
    return;
  Java_Shell_onNativeDestroyed(env, java_object_.obj());
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  JNIEnv* env = AttachCurrentThread();
  if (java_object_.is_null())
    return;
  Java_Shell_enableUiControl(env, java_object_.obj(), control, is_enabled);
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  Java_Shell_onUpdateUrl(env, java_object_.obj(), j_url.obj());
}

void Shell::PlatformSetIsLoading(bool loading) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_setIsLoading(env, java_object_.obj(), loading);
}

void Shell::PlatformCreateWindow(int width, int height) {
  java_object_.Reset(AttachCurrentThread(), CreateShellView(this));
}

void Shell::PlatformSetContents() {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_initFromNativeTabContents(
      env, java_object_.obj(), reinterpret_cast<intptr_t>(web_contents()));
}

void Shell::PlatformResizeSubViews() {
  // Not needed; subviews are bound.
}

void Shell::PlatformSetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void Shell::LoadProgressChanged(WebContents* source, double progress) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_onLoadProgressChanged(env, java_object_.obj(), progress);
}

void Shell::PlatformToggleFullscreenModeForTab(WebContents* web_contents,
                                               bool enter_fullscreen) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_toggleFullscreenModeForTab(
      env, java_object_.obj(), enter_fullscreen);
}

bool Shell::PlatformIsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  JNIEnv* env = AttachCurrentThread();
  return Java_Shell_isFullscreenForTabOrPending(env, java_object_.obj());
}

bool Shell::PlatformHandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

void Shell::Close() {
  RemoveShellView(java_object_.obj());
  delete this;
}

// static
bool Shell::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void CloseShell(JNIEnv* env, jclass clazz, jlong shellPtr) {
  Shell* shell = reinterpret_cast<Shell*>(shellPtr);
  shell->Close();
}

}  // namespace content
