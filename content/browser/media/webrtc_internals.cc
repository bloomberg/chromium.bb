// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_internals.h"

#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using base::DictionaryValue;
using base::ListValue;
using base::ProcessId;
using std::string;

namespace content {

namespace {
// Makes sure that |dict| has a ListValue under path "log".
static ListValue* EnsureLogList(DictionaryValue* dict) {
  ListValue* log = NULL;
  if (!dict->GetList("log", &log)) {
    log = new ListValue();
    if (log)
      dict->Set("log", log);
  }
  return log;
}

}  // namespace

WebRTCInternals::WebRTCInternals() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());

  BrowserChildProcessObserver::Add(this);
}

WebRTCInternals::~WebRTCInternals() {
  BrowserChildProcessObserver::Remove(this);
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  return Singleton<WebRTCInternals>::get();
}

void WebRTCInternals::AddPeerConnection(int render_process_id,
                                        ProcessId pid,
                                        int lid,
                                        const string& url,
                                        const string& servers,
                                        const string& constraints) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue* dict = new DictionaryValue();
  if (!dict)
    return;

  dict->SetInteger("rid", render_process_id);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);
  dict->SetString("servers", servers);
  dict->SetString("constraints", constraints);
  dict->SetString("url", url);
  peer_connection_data_.Append(dict);

  if (observers_.size() > 0)
    SendUpdate("addPeerConnection", dict);
}

void WebRTCInternals::RemovePeerConnection(ProcessId pid, int lid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
    DictionaryValue* dict = NULL;
    peer_connection_data_.GetDictionary(i, &dict);

    int this_pid = 0;
    int this_lid = 0;
    dict->GetInteger("pid", &this_pid);
    dict->GetInteger("lid", &this_lid);

    if (this_pid != static_cast<int>(pid) || this_lid != lid)
      continue;

    peer_connection_data_.Remove(i, NULL);

    if (observers_.size() > 0) {
      DictionaryValue id;
      id.SetInteger("pid", static_cast<int>(pid));
      id.SetInteger("lid", lid);
      SendUpdate("removePeerConnection", &id);
    }
    break;
  }
}

void WebRTCInternals::UpdatePeerConnection(
    ProcessId pid, int lid, const string& type, const string& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
    DictionaryValue* record = NULL;
    peer_connection_data_.GetDictionary(i, &record);

    int this_pid = 0, this_lid = 0;
    record->GetInteger("pid", &this_pid);
    record->GetInteger("lid", &this_lid);

    if (this_pid != static_cast<int>(pid) || this_lid != lid)
      continue;

    // Append the update to the end of the log.
    ListValue* log = EnsureLogList(record);
    if (!log)
      return;

    DictionaryValue* log_entry = new DictionaryValue();
    if (!log_entry)
      return;

    log_entry->SetString("type", type);
    log_entry->SetString("value", value);
    log->Append(log_entry);

    if (observers_.size() > 0) {
      DictionaryValue update;
      update.SetInteger("pid", static_cast<int>(pid));
      update.SetInteger("lid", lid);
      update.SetString("type", type);
      update.SetString("value", value);

      SendUpdate("updatePeerConnection", &update);
    }
    return;
  }
}

void WebRTCInternals::AddStats(base::ProcessId pid, int lid,
                               const base::ListValue& value) {
  if (observers_.size() == 0)
    return;

  DictionaryValue dict;
  dict.SetInteger("pid", static_cast<int>(pid));
  dict.SetInteger("lid", lid);

  ListValue* list = value.DeepCopy();
  if (!list)
    return;

  dict.Set("reports", list);

  SendUpdate("addStats", &dict);
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void WebRTCInternals::SendAllUpdates() {
  if (observers_.size() > 0)
    SendUpdate("updateAllPeerConnections", &peer_connection_data_);
}

void WebRTCInternals::SendUpdate(const string& command, Value* value) {
  DCHECK_GT(observers_.size(), (size_t)0);

  FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                    observers_,
                    OnUpdate(command, value));
}

void WebRTCInternals::BrowserChildProcessCrashed(
    const ChildProcessData& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  OnRendererExit(data.id);
}

void WebRTCInternals::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  OnRendererExit(Source<RenderProcessHost>(source)->GetID());
}

void WebRTCInternals::OnRendererExit(int render_process_id) {
  // Iterates from the end of the list to remove the PeerConnections created
  // by the exitting renderer.
  for (int i = peer_connection_data_.GetSize() - 1; i >= 0; --i) {
    DictionaryValue* record = NULL;
    peer_connection_data_.GetDictionary(i, &record);

    int this_rid = 0;
    record->GetInteger("rid", &this_rid);

    if (this_rid == render_process_id) {
      if (observers_.size() > 0) {
        int lid = 0, pid = 0;
        record->GetInteger("lid", &lid);
        record->GetInteger("pid", &pid);

        DictionaryValue update;
        update.SetInteger("lid", lid);
        update.SetInteger("pid", pid);
        SendUpdate("removePeerConnection", &update);
      }
      peer_connection_data_.Remove(i, NULL);
    }
  }
}

}  // namespace content
