// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/android_library_loader_hooks.h"
#include "content/shell/shell_main_delegate.h"
#include "content/shell/android/shell_manager.h"
#include "content/shell/android/shell_view.h"

static base::android::RegistrationMethod kRegistrationMethods[] = {
    { "ShellManager", content::RegisterShellManager },
    { "ShellView", content::ShellView::Register },
};

namespace {
  content::ContentMainRunner* g_content_main_runner = NULL;
}

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

  // Don't call anything in base without initializing it.
  // ContentMainRunner will do what we need.
  g_content_main_runner = content::ContentMainRunner::Create();

  // TODO(tedchoc): Set this to the main delegate once the Android specific
  // browser process initialization gets checked in.
  ShellMainDelegate* delegate = new ShellMainDelegate();

  // We use a ShellContentClient, created as a member of
  // ShellMainDelegate and set with a call to
  // content::SetContentClient() in PreSandboxStartup().
  // That must be done before ContentMainRunner::Initialize().
  // TODO(jrg): resolve the upstream/downstream discrepancy; we
  // shouldn't need to do this.
  delegate->PreSandboxStartup();

  // TODO(jrg): find command line info from java; pass down in here.
  g_content_main_runner->Initialize(0, NULL, NULL);

  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterLibraryLoaderEntryHook(env)) {
    return -1;
  }

  // To be called only from the UI thread.  If loading the library is done on
  // a separate thread, this should be moved elsewhere.
  if (!base::android::RegisterNativeMethods(env, kRegistrationMethods,
                                            arraysize(kRegistrationMethods)))
    return -1;

  return JNI_VERSION_1_4;
}


JNI_EXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
  delete g_content_main_runner;
  g_content_main_runner = NULL;
}

