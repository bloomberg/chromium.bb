// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_internals.h"

#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"

using base::DictionaryValue;
using base::ProcessId;

namespace content{

WebRTCInternals::WebRTCInternals() {
}

WebRTCInternals::~WebRTCInternals() {
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  return Singleton<WebRTCInternals>::get();
}

void WebRTCInternals::AddPeerConnection(ProcessId pid,
                                        const PeerConnectionInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (observers_.size()) {
    DictionaryValue* dict = new DictionaryValue();
    if (dict != NULL) {
      dict->SetInteger("pid", static_cast<int>(pid));
      dict->SetInteger("lid", info.lid);
      dict->SetString("servers", info.servers);
      dict->SetString("constraints", info.constraints);
      dict->SetString("url", info.url);

      SendUpdate("updatePeerConnectionAdded", dict);
      peer_connection_data_.Append(dict);
    }
  }
}

void WebRTCInternals::RemovePeerConnection(ProcessId pid, int lid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (observers_.size()) {
    DictionaryValue dict;
    dict.SetInteger("pid", static_cast<int>(pid));
    dict.SetInteger("lid", lid);
    SendUpdate("updatePeerConnectionRemoved", &dict);

    for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
      DictionaryValue* dict = NULL;
      peer_connection_data_.GetDictionary(i, &dict);

      int this_pid = 0;
      int this_lid = 0;
      dict->GetInteger("pid", &this_pid);
      dict->GetInteger("lid", &this_lid);
      if (this_pid == static_cast<int>(pid) && this_lid == lid)
        peer_connection_data_.Remove(i, NULL);
    }
  }
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.RemoveObserver(observer);
}

void WebRTCInternals::SendUpdate(const std::string& command, Value* value) {
  DCHECK(observers_.size());

  FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                    observers_,
                    OnUpdate(command, value));
}

}  // namespace content
