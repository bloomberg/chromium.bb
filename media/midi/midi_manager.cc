// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
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
  // Lazily initialize the MIDI back-end.
  if (!initialized_) {
    initialized_ = true;
    result_ = Initialize();
  }

  if (result_ == MIDI_OK) {
    base::AutoLock auto_lock(clients_lock_);
    clients_.insert(client);
  }
  // TODO(toyoshim): Make Initialize() asynchronous.
  client->CompleteStartSession(client_id, result_);
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

MidiResult MidiManager::Initialize() {
  TRACE_EVENT0("midi", "MidiManager::Initialize");
  return MIDI_NOT_SUPPORTED;
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
