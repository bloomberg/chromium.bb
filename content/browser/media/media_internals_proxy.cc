// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_proxy.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/media_internals_handler.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"

namespace content {

MediaInternalsProxy::MediaInternalsProxy() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
}

void MediaInternalsProxy::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(type, NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
  CallJavaScriptFunctionOnUIThread(
      "media.onRendererTerminated",
      base::MakeUnique<base::Value>(process->GetID()));
}

void MediaInternalsProxy::Attach(MediaInternalsMessageHandler* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  handler_ = handler;
  update_callback_ = base::Bind(&MediaInternalsProxy::UpdateUIOnUIThread, this);
  MediaInternals::GetInstance()->AddUpdateCallback(update_callback_);
}

void MediaInternalsProxy::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  handler_ = nullptr;
  MediaInternals::GetInstance()->RemoveUpdateCallback(update_callback_);
}

void MediaInternalsProxy::GetEverything() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaInternals::GetInstance()->SendHistoricalMediaEvents();

  // Ask MediaInternals for its data on IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsProxy::GetEverythingOnIOThread, this));
}

MediaInternalsProxy::~MediaInternalsProxy() {}

void MediaInternalsProxy::GetEverythingOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(xhwang): Investigate whether we can update on UI thread directly.
  MediaInternals::GetInstance()->SendAudioStreamData();
  MediaInternals::GetInstance()->SendVideoCaptureDeviceCapabilities();
}

void MediaInternalsProxy::UpdateUIOnUIThread(const base::string16& update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}

void MediaInternalsProxy::CallJavaScriptFunctionOnUIThread(
    const std::string& function,
    std::unique_ptr<base::Value> args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<const base::Value*> args_vector;
  args_vector.push_back(args.get());
  base::string16 update = WebUI::GetJavascriptCall(function, args_vector);
  UpdateUIOnUIThread(update);
}

}  // namespace content
