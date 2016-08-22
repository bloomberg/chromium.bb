// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>

#include "android_webview/common/aw_descriptors.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/sync_socket.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

namespace android_webview {

AwBrowserTerminator::AwBrowserTerminator() {}

AwBrowserTerminator::~AwBrowserTerminator() {}

void AwBrowserTerminator::OnChildStart(int child_process_id,
                                       content::FileDescriptorInfo* mappings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::PROCESS_LAUNCHER);

  base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
  DCHECK(!ContainsKey(child_process_id_to_pipe_, child_process_id));

  std::unique_ptr<base::SyncSocket> local_pipe(new base::SyncSocket());
  std::unique_ptr<base::SyncSocket> child_pipe(new base::SyncSocket());
  if (base::SyncSocket::CreatePair(local_pipe.get(), child_pipe.get())) {
    child_process_id_to_pipe_[child_process_id] = std::move(local_pipe);
    mappings->Transfer(kAndroidWebViewCrashSignalDescriptor,
                       base::ScopedFD(dup(child_pipe->handle())));
  }
}

void AwBrowserTerminator::OnChildExit(
    int child_process_id,
    base::ProcessHandle pid,
    content::ProcessType process_type,
    base::TerminationStatus termination_status,
    base::android::ApplicationState app_state) {
  std::unique_ptr<base::SyncSocket> pipe;

  {
    base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
    const auto& iter = child_process_id_to_pipe_.find(child_process_id);
    if (iter == child_process_id_to_pipe_.end()) {
      // We might get a NOTIFICATION_RENDERER_PROCESS_TERMINATED and a
      // NOTIFICATION_RENDERER_PROCESS_CLOSED.
      return;
    }
    pipe = std::move(iter->second);
    child_process_id_to_pipe_.erase(iter);
  }
  DCHECK(pipe->handle() != base::SyncSocket::kInvalidHandle);

  if (termination_status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;
  if (pipe->Peek() >= sizeof(int)) {
    int exit_code;
    pipe->Receive(&exit_code, sizeof(exit_code));
    LOG(FATAL) << "Renderer process crash detected (code " << exit_code
               << "). Terminating browser.";
  } else {
    // The child process hasn't written anything into the pipe. This implies
    // that it was terminated via SIGKILL by the low memory killer, and thus we
    // need to perform a clean exit.
    exit(0);
  }
}

}  // namespace breakpad
