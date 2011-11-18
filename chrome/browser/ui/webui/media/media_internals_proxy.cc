// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_internals_proxy.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/ui/webui/media/media_internals_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

static const int kMediaInternalsProxyEventDelayMilliseconds = 100;

static const net::NetLog::EventType kNetEventTypeFilter[] = {
  net::NetLog::TYPE_DISK_CACHE_ENTRY_IMPL,
  net::NetLog::TYPE_SPARSE_READ,
  net::NetLog::TYPE_SPARSE_WRITE,
  net::NetLog::TYPE_URL_REQUEST_START_JOB,
  net::NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
};

MediaInternalsProxy::MediaInternalsProxy()
    : ThreadSafeObserverImpl(net::NetLog::LOG_ALL_BUT_BYTES) {
  io_thread_ = g_browser_process->io_thread();
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void MediaInternalsProxy::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  CallJavaScriptFunctionOnUIThread("media.onRendererTerminated",
      base::Value::CreateIntegerValue(process->GetID()));
}

void MediaInternalsProxy::Attach(MediaInternalsMessageHandler* handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = handler;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsProxy::ObserveMediaInternalsOnIOThread, this));
}

void MediaInternalsProxy::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = NULL;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &MediaInternalsProxy::StopObservingMediaInternalsOnIOThread, this));
}

void MediaInternalsProxy::GetEverything() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ask MediaInternals for all its data.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsProxy::GetEverythingOnIOThread, this));

  // Send the page names for constants.
  CallJavaScriptFunctionOnUIThread("media.onReceiveConstants", GetConstants());
}

void MediaInternalsProxy::OnUpdate(const string16& update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaInternalsProxy::UpdateUIOnUIThread, this, update));
}

void MediaInternalsProxy::OnAddEntry(net::NetLog::EventType type,
                                     const base::TimeTicks& time,
                                     const net::NetLog::Source& source,
                                     net::NetLog::EventPhase phase,
                                     net::NetLog::EventParameters* params) {
  bool is_event_interesting = false;
  for (size_t i = 0; i < arraysize(kNetEventTypeFilter); i++) {
    if (type == kNetEventTypeFilter[i]) {
      is_event_interesting = true;
      break;
    }
  }

  if (!is_event_interesting)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaInternalsProxy::AddNetEventOnUIThread, this,
          net::NetLog::EntryToDictionaryValue(type, time, source, phase,
                                              params, false)));
}

MediaInternalsProxy::~MediaInternalsProxy() {}

Value* MediaInternalsProxy::GetConstants() {
  DictionaryValue* event_types = new DictionaryValue();
  std::vector<net::NetLog::EventType> types_vec =
      net::NetLog::GetAllEventTypes();

  for (size_t i = 0; i < types_vec.size(); ++i) {
    const char* name = net::NetLog::EventTypeToString(types_vec[i]);
    event_types->SetInteger(name, static_cast<int>(types_vec[i]));
  }

  DictionaryValue* event_phases = new DictionaryValue();
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLog::PHASE_NONE),
      net::NetLog::PHASE_NONE);
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLog::PHASE_BEGIN),
      net::NetLog::PHASE_BEGIN);
  event_phases->SetInteger(
      net::NetLog::EventPhaseToString(net::NetLog::PHASE_END),
      net::NetLog::PHASE_END);

  DictionaryValue* constants = new DictionaryValue();
  constants->Set("eventTypes", event_types);
  constants->Set("eventPhases", event_phases);

  return constants;
}

void MediaInternalsProxy::ObserveMediaInternalsOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_thread_->globals()->media.media_internals->AddObserver(this);
  // TODO(scottfr): Get the passive log data as well.
  AddAsObserver(io_thread_->net_log());
}

void MediaInternalsProxy::StopObservingMediaInternalsOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_thread_->globals()->media.media_internals->RemoveObserver(this);
  RemoveAsObserver();
}

void MediaInternalsProxy::GetEverythingOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_thread_->globals()->media.media_internals->SendEverything();
}

void MediaInternalsProxy::UpdateUIOnUIThread(const string16& update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}

void MediaInternalsProxy::AddNetEventOnUIThread(Value* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Send the updates to the page in kMediaInternalsProxyEventDelayMilliseconds
  // if an update is not already pending.
  if (!pending_net_updates_.get()) {
    pending_net_updates_.reset(new ListValue());
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &MediaInternalsProxy::SendNetEventsOnUIThread, this),
        kMediaInternalsProxyEventDelayMilliseconds);
  }
  pending_net_updates_->Append(entry);
}

void MediaInternalsProxy::SendNetEventsOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CallJavaScriptFunctionOnUIThread("media.onNetUpdate",
                                   pending_net_updates_.release());
}

void MediaInternalsProxy::CallJavaScriptFunctionOnUIThread(
    const std::string& function, Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<Value> args_value(args);
  std::vector<const Value*> args_vector;
  args_vector.push_back(args_value.get());
  string16 update = WebUI::GetJavascriptCall(function, args_vector);
  UpdateUIOnUIThread(update);
}
