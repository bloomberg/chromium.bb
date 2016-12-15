// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_service.h"

#include "media/midi/midi_manager.h"

namespace midi {

MidiService::MidiService(std::unique_ptr<MidiManager> manager) {
  base::AutoLock lock(lock_);
  if (manager.get())
    manager_ = std::move(manager);
  else
    manager_.reset(MidiManager::Create());
}

MidiService::~MidiService() {
  base::AutoLock lock(lock_);
  manager_.reset();
}

void MidiService::Shutdown() {
  base::AutoLock lock(lock_);
  manager_->Shutdown();
}

void MidiService::StartSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);
  manager_->StartSession(client);
}

void MidiService::EndSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);
  manager_->EndSession(client);
}

void MidiService::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32_t port_index,
                                       const std::vector<uint8_t>& data,
                                       double timestamp) {
  base::AutoLock lock(lock_);
  manager_->DispatchSendMidiData(client, port_index, data, timestamp);
}

}  // namespace midi
