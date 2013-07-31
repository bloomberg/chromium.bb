// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread.h"

namespace media {

#if !defined(OS_MACOSX)
// TODO(crogers): implement MIDIManager for other platforms.
MIDIManager* MIDIManager::Create() {
  return NULL;
}
#endif

MIDIManager::MIDIManager()
    : initialized_(false) {
}

MIDIManager::~MIDIManager() {}

bool MIDIManager::StartSession(MIDIManagerClient* client) {
  // Lazily initialize the MIDI back-end.
  if (!initialized_)
    initialized_ = Initialize();

  if (initialized_) {
    base::AutoLock auto_lock(clients_lock_);
    clients_.insert(client);
  }

  return initialized_;
}

void MIDIManager::EndSession(MIDIManagerClient* client) {
  base::AutoLock auto_lock(clients_lock_);
  ClientList::iterator i = clients_.find(client);
  if (i != clients_.end())
    clients_.erase(i);
}

void MIDIManager::AddInputPort(const MIDIPortInfo& info) {
  input_ports_.push_back(info);
}

void MIDIManager::AddOutputPort(const MIDIPortInfo& info) {
  output_ports_.push_back(info);
}

void MIDIManager::ReceiveMIDIData(
    int port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  base::AutoLock auto_lock(clients_lock_);

  for (ClientList::iterator i = clients_.begin(); i != clients_.end(); ++i)
    (*i)->ReceiveMIDIData(port_index, data, length, timestamp);
}

void MIDIManager::DispatchSendMIDIData(MIDIManagerClient* client,
                                       int port_index,
                                       const uint8* data,
                                       size_t length,
                                       double timestamp) {
  // Lazily create the thread when first needed.
  if (!send_thread_) {
    send_thread_.reset(new base::Thread("MIDISendThread"));
    send_thread_->Start();
    send_message_loop_ = send_thread_->message_loop_proxy();
  }

  send_message_loop_->PostTask(
     FROM_HERE,
     base::Bind(&MIDIManager::SendMIDIData, base::Unretained(this),
         client, port_index, data, length, timestamp));
}

}  // namespace media
