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
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"

namespace content {

static const int kMediaInternalsProxyEventDelayMilliseconds = 100;

static const net::NetLogEventType kNetEventTypeFilter[] = {
    net::NetLogEventType::DISK_CACHE_ENTRY_IMPL,
    net::NetLogEventType::SPARSE_READ,
    net::NetLogEventType::SPARSE_WRITE,
    net::NetLogEventType::URL_REQUEST_START_JOB,
    net::NetLogEventType::HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
};

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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsProxy::ObserveMediaInternalsOnIOThread, this));
}

void MediaInternalsProxy::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  handler_ = NULL;
  MediaInternals::GetInstance()->RemoveUpdateCallback(update_callback_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &MediaInternalsProxy::StopObservingMediaInternalsOnIOThread, this));
}

void MediaInternalsProxy::GetEverything() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaInternals::GetInstance()->SendHistoricalMediaEvents();

  // Ask MediaInternals for its data on IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsProxy::GetEverythingOnIOThread, this));

  // Send the page names for constants.
  CallJavaScriptFunctionOnUIThread("media.onReceiveConstants", GetConstants());
}

void MediaInternalsProxy::OnAddEntry(const net::NetLogEntry& entry) {
  bool is_event_interesting = false;
  for (size_t i = 0; i < arraysize(kNetEventTypeFilter); i++) {
    if (entry.type() == kNetEventTypeFilter[i]) {
      is_event_interesting = true;
      break;
    }
  }

  if (!is_event_interesting)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaInternalsProxy::AddNetEventOnUIThread, this,
                 base::Passed(entry.ToValue())));
}

MediaInternalsProxy::~MediaInternalsProxy() {}

std::unique_ptr<base::Value> MediaInternalsProxy::GetConstants() {
  auto event_phases = base::MakeUnique<base::DictionaryValue>();
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLogEventPhase::NONE),
      static_cast<int>(net::NetLogEventPhase::NONE));
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLogEventPhase::BEGIN),
      static_cast<int>(net::NetLogEventPhase::BEGIN));
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLogEventPhase::END),
      static_cast<int>(net::NetLogEventPhase::END));

  auto constants = base::MakeUnique<base::DictionaryValue>();
  constants->Set("eventTypes", net::NetLog::GetEventTypesAsValue());
  constants->Set("eventPhases", std::move(event_phases));

  return std::move(constants);
}

void MediaInternalsProxy::ObserveMediaInternalsOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetContentClient()->browser()->GetNetLog()) {
    net::NetLog* net_log = GetContentClient()->browser()->GetNetLog();
    net_log->DeprecatedAddObserver(
        this, net::NetLogCaptureMode::IncludeCookiesAndCredentials());
  }
}

void MediaInternalsProxy::StopObservingMediaInternalsOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetContentClient()->browser()->GetNetLog()) {
    net::NetLog* net_log = GetContentClient()->browser()->GetNetLog();
    net_log->DeprecatedRemoveObserver(this);
  }
}

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

void MediaInternalsProxy::AddNetEventOnUIThread(
    std::unique_ptr<base::Value> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Send the updates to the page in kMediaInternalsProxyEventDelayMilliseconds
  // if an update is not already pending.
  if (!pending_net_updates_) {
    pending_net_updates_.reset(new base::ListValue());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaInternalsProxy::SendNetEventsOnUIThread, this),
        base::TimeDelta::FromMilliseconds(
            kMediaInternalsProxyEventDelayMilliseconds));
  }
  pending_net_updates_->Append(std::move(entry));
}

void MediaInternalsProxy::SendNetEventsOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallJavaScriptFunctionOnUIThread("media.onNetUpdate",
                                   std::move(pending_net_updates_));
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
