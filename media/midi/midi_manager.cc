// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/debug/trace_event.h"

namespace media {

#if !defined(OS_MACOSX) && !defined(OS_WIN) && !defined(USE_ALSA) && \
    !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
MidiManager* MidiManager::Create() {
  return new MidiManager;
}
#endif

MidiManager::MidiManager()
    : initialized_(false),
      result_(MIDI_NOT_SUPPORTED) {
}

MidiManager::~MidiManager() {
}

void MidiManager::StartSession(MidiManagerClient* client, int client_id) {
  bool session_is_ready;
  bool session_needs_initialization = false;

  {
    base::AutoLock auto_lock(clients_lock_);
    session_is_ready = initialized_;
    if (!session_is_ready) {
      // Call StartInitialization() only for the first request.
      session_needs_initialization = pending_clients_.empty();
      pending_clients_.insert(
          std::pair<int, MidiManagerClient*>(client_id, client));
    }
  }

  // Lazily initialize the MIDI back-end.
  if (!session_is_ready) {
    if (session_needs_initialization) {
      TRACE_EVENT0("midi", "MidiManager::StartInitialization");
      StartInitialization();
    }
    // CompleteInitialization() will be called asynchronously when platform
    // dependent initialization is finished.
    return;
  }

  // Platform dependent initialization was already finished for previously
  // initialized clients.
  MidiResult result;
  {
    base::AutoLock auto_lock(clients_lock_);
    if (result_ == MIDI_OK)
      clients_.insert(client);
    result = result_;
  }
  client->CompleteStartSession(client_id, result);
}

void MidiManager::EndSession(MidiManagerClient* client) {
  base::AutoLock auto_lock(clients_lock_);
  ClientList::iterator i = clients_.find(client);
  if (i != clients_.end())
    clients_.erase(i);
}

void MidiManager::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32 port_index,
                                       const std::vector<uint8>& data,
                                       double timestamp) {
  NOTREACHED();
}

void MidiManager::StartInitialization() {
  CompleteInitialization(MIDI_NOT_SUPPORTED);
}

void MidiManager::CompleteInitialization(MidiResult result) {
  TRACE_EVENT0("midi", "MidiManager::CompleteInitialization");

  base::AutoLock auto_lock(clients_lock_);
  DCHECK(clients_.empty());
  DCHECK(!pending_clients_.empty());
  DCHECK(!initialized_);
  initialized_ = true;
  result_ = result;

  for (PendingClientMap::iterator it = pending_clients_.begin();
       it != pending_clients_.end();
       ++it) {
    if (result_ == MIDI_OK)
      clients_.insert(it->second);
    it->second->CompleteStartSession(it->first, result_);
  }
  pending_clients_.clear();
}

void MidiManager::AddInputPort(const MidiPortInfo& info) {
  input_ports_.push_back(info);
}

void MidiManager::AddOutputPort(const MidiPortInfo& info) {
  output_ports_.push_back(info);
}

void MidiManager::ReceiveMidiData(
    uint32 port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  base::AutoLock auto_lock(clients_lock_);

  for (ClientList::iterator i = clients_.begin(); i != clients_.end(); ++i)
    (*i)->ReceiveMidiData(port_index, data, length, timestamp);
}

}  // namespace media
