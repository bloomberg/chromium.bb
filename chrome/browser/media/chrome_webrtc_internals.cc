// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chrome_webrtc_internals.h"

#include "chrome/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/browser/browser_thread.h"

using base::DictionaryValue;
using base::ListValue;
using base::ProcessId;
using content::BrowserThread;
using std::string;

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

ChromeWebRTCInternals::ChromeWebRTCInternals() {
}

ChromeWebRTCInternals::~ChromeWebRTCInternals() {
}

ChromeWebRTCInternals* ChromeWebRTCInternals::GetInstance() {
  return Singleton<ChromeWebRTCInternals>::get();
}

void ChromeWebRTCInternals::AddPeerConnection(ProcessId pid,
                                        int lid,
                                        const string& url,
                                        const string& servers,
                                        const string& constraints) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue* dict = new DictionaryValue();
  if (!dict)
    return;

  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);
  dict->SetString("servers", servers);
  dict->SetString("constraints", constraints);
  dict->SetString("url", url);
  peer_connection_data_.Append(dict);

  if (observers_.size() > 0)
    SendUpdate("addPeerConnection", dict);
}

void ChromeWebRTCInternals::RemovePeerConnection(ProcessId pid, int lid) {
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

void ChromeWebRTCInternals::UpdatePeerConnection(
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

void ChromeWebRTCInternals::AddObserver(
      WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void ChromeWebRTCInternals::RemoveObserver(
      WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void ChromeWebRTCInternals::SendAllUpdates() {
  if (observers_.size() > 0)
    SendUpdate("updateAllPeerConnections", &peer_connection_data_);
}

void ChromeWebRTCInternals::SendUpdate(const string& command, Value* value) {
  DCHECK_GT(observers_.size(), (size_t)0);

  FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                    observers_,
                    OnUpdate(command, value));
}
