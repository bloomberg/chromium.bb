// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/android/content_view_statics.h"
#include "content/common/android/address_parser.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "jni/ContentViewStatics_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;

namespace {

// TODO(pliard): http://crbug.com/235909. Move WebKit shared timer toggling
// functionality out of ContentViewStatistics and not be build on top of
// WebKit::Platform::SuspendSharedTimer.
// TODO(pliard): http://crbug.com/235912. Add unit tests for WebKit shared timer
// toggling.

// This tracks the renderer processes that received a suspend request. It's
// important on resume to only resume the renderer processes that were actually
// suspended as opposed to all the current renderer processes because the
// suspend calls are refcounted within WebKitPlatformSupport and it expects a
// perfectly matched number of resume calls.
// Note that this vector is only accessed from the UI thread.
base::LazyInstance<std::vector<int /* process id */> > g_suspended_processes =
    LAZY_INSTANCE_INITIALIZER;

// Suspends timers in all current render processes.
void SuspendWebKitSharedTimers(std::vector<int>* suspended_processes) {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    suspended_processes->push_back(host->GetID());
    host->Send(new ViewMsg_SetWebKitSharedTimersSuspended(true));
  }
}

// Resumes timers in processes that were previously stopped.
void ResumeWebkitSharedTimers(const std::vector<int>& suspended_processes) {
  for (std::vector<int>::const_iterator it = suspended_processes.begin();
       it != suspended_processes.end(); ++it) {
    content::RenderProcessHost* host = content::RenderProcessHost::FromID(*it);
    if (host)  // The process might have been killed since it was suspended.
      host->Send(new ViewMsg_SetWebKitSharedTimersSuspended(false));
  }
}

}  // namespace

// Returns the first substring consisting of the address of a physical location.
static jstring FindAddress(JNIEnv* env, jclass clazz, jstring addr) {
  string16 content_16 = ConvertJavaStringToUTF16(env, addr);
  string16 result_16;
  if (content::address_parser::FindAddress(content_16, &result_16))
    return ConvertUTF16ToJavaString(env, result_16).Release();
  return NULL;
}

static void SetWebKitSharedTimersSuspended(JNIEnv* env,
                                           jclass obj,
                                           jboolean suspend) {
  std::vector<int>* suspended_processes = g_suspended_processes.Pointer();
  if (suspend) {
    DCHECK(suspended_processes->empty());
    SuspendWebKitSharedTimers(suspended_processes);
  } else {
    ResumeWebkitSharedTimers(*suspended_processes);
    suspended_processes->clear();
  }
}

namespace content {

bool RegisterWebViewStatics(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace content
