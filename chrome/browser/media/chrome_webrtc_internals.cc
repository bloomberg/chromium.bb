// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chrome_webrtc_internals.h"

#include "chrome/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/browser/browser_thread.h"

using base::DictionaryValue;
using base::ProcessId;
using content::BrowserThread;
using std::string;

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

void ChromeWebRTCInternals::SendUpdate(const string& command, Value* value) {
  DCHECK(observers_.size() > 0);

  FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                    observers_,
                    OnUpdate(command, value));
}
