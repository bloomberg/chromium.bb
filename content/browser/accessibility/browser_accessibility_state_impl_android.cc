// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "jni/BrowserAccessibilityState_jni.h"

using base::android::AttachCurrentThread;

namespace content {

void BrowserAccessibilityStateImpl::PlatformInitialize() {}

void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
  // NOTE: this method is run from the file thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Be careful
  // not to add any code that isn't safe to run from a non-main thread!
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  JNIEnv* env = AttachCurrentThread();
  Java_BrowserAccessibilityState_recordAccessibilityHistograms(env);
}

}  // namespace content
