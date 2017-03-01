// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>

#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/crash_reporter/aw_microdump_crash_reporter.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/sync_socket.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void GetAwRenderProcessGoneDelegatesForRenderProcess(
    int render_process_id,
    std::vector<AwRenderProcessGoneDelegate*>* delegates) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderViewHost* view = content::RenderViewHost::From(widget);
    if (view && rph == view->GetProcess()) {
      content::WebContents* wc = content::WebContents::FromRenderViewHost(view);
      if (wc) {
        AwRenderProcessGoneDelegate* delegate =
            AwRenderProcessGoneDelegate::FromWebContents(wc);
        if (delegate)
          delegates->push_back(delegate);
      }
    }
  }
}

void OnRenderProcessGone(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(child_process_id, &delegates);
  for (auto* delegate : delegates)
    delegate->OnRenderProcessGone(child_process_id);
}

void OnRenderProcessGoneDetail(int child_process_id,
                               base::ProcessHandle child_process_pid,
                               bool crashed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(child_process_id, &delegates);
  for (auto* delegate : delegates) {
    if (!delegate->OnRenderProcessGoneDetail(child_process_pid, crashed)) {
      if (crashed) {
        // Keeps this log unchanged, CTS test uses it to detect crash.
        LOG(FATAL) << "Render process (" << child_process_pid << ")'s crash"
                   << " wasn't handled by all associated  webviews, triggering"
                   << " application crash.";
      } else {
        // The render process was most likely killed for OOM or switching
        // WebView provider, to make WebView backward compatible, kills the
        // browser process instead of triggering crash.
        LOG(ERROR) << "Render process (" << child_process_pid << ") kill (OOM"
                   << " or update) wasn't handed by all associated webviews,"
                   << " killing application.";
        kill(getpid(), SIGKILL);
      }
    }
  }
}

}  // namespace

AwBrowserTerminator::AwBrowserTerminator() {}

AwBrowserTerminator::~AwBrowserTerminator() {}

void AwBrowserTerminator::OnChildStart(int child_process_id,
                                       content::FileDescriptorInfo* mappings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::PROCESS_LAUNCHER);

  base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
  DCHECK(!ContainsKey(child_process_id_to_pipe_, child_process_id));

  auto local_pipe = base::MakeUnique<base::SyncSocket>();
  auto child_pipe = base::MakeUnique<base::SyncSocket>();
  if (base::SyncSocket::CreatePair(local_pipe.get(), child_pipe.get())) {
    child_process_id_to_pipe_[child_process_id] = std::move(local_pipe);
    mappings->Transfer(kAndroidWebViewCrashSignalDescriptor,
                       base::ScopedFD(dup(child_pipe->handle())));
  }
}

void AwBrowserTerminator::ProcessTerminationStatus(
    int child_process_id,
    base::ProcessHandle pid,
    std::unique_ptr<base::SyncSocket> pipe) {
  bool crashed = false;

  // If the child process hasn't written anything into the pipe. This implies
  // that it was terminated via SIGKILL by the low memory killer.
  if (pipe->Peek() >= sizeof(int)) {
    int exit_code;
    pipe->Receive(&exit_code, sizeof(exit_code));
    crash_reporter::SuppressDumpGeneration();
    LOG(ERROR) << "Renderer process (" << pid << ") crash detected (code "
               << exit_code << ").";
    crashed = true;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnRenderProcessGoneDetail, child_process_id, pid, crashed));
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
  if (termination_status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;
  OnRenderProcessGone(child_process_id);
  DCHECK(pipe->handle() != base::SyncSocket::kInvalidHandle);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AwBrowserTerminator::ProcessTerminationStatus,
                 child_process_id, pid, base::Passed(std::move(pipe))));
}

}  // namespace breakpad
