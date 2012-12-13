// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/process_utils.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_process_host.h"
#include "jni/ProcessUtils_jni.h"

namespace {

// Only accessed from the JNI thread by ToggleWebKitSharedTimers() which is
// implemented below.
base::LazyInstance<std::vector<int /* process id */> > g_suspended_processes =
    LAZY_INSTANCE_INITIALIZER;

// Suspends timers in all current render processes.
void SuspendWebKitSharedTimers(std::vector<int>* suspended_processes) {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    suspended_processes->push_back(host->GetID());
    host->Send(new ChromeViewMsg_ToggleWebKitSharedTimer(true));
  }
}

void ResumeWebkitSharedTimers(const std::vector<int>& suspended_processes) {
  for (std::vector<int>::const_iterator it = suspended_processes.begin();
       it != suspended_processes.end(); ++it) {
    content::RenderProcessHost* host = content::RenderProcessHost::FromID(*it);
    if (host)
      host->Send(new ChromeViewMsg_ToggleWebKitSharedTimer(false));
  }
}

}  // namespace

static void ToggleWebKitSharedTimers(JNIEnv* env,
                                     jclass obj,
                                     jboolean suspend) {
  std::vector<int>* suspended_processes = &g_suspended_processes.Get();
  if (suspend) {
    DCHECK(suspended_processes->empty());
    SuspendWebKitSharedTimers(suspended_processes);
  } else {
    ResumeWebkitSharedTimers(*suspended_processes);
    suspended_processes->clear();
  }
}

bool RegisterProcessUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
