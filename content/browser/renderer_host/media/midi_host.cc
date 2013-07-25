// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/midi_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/process/process.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_internals.h"
#include "content/common/media/midi_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "media/midi/midi_manager.h"

using media::MIDIManager;
using media::MIDIPortInfoList;

namespace content {

MIDIHost::MIDIHost(media::MIDIManager* midi_manager)
    : midi_manager_(midi_manager) {
}

MIDIHost::~MIDIHost() {
  if (midi_manager_)
    midi_manager_->EndSession(this);
}

void MIDIHost::OnChannelClosing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserMessageFilter::OnChannelClosing();
}

void MIDIHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler
bool MIDIHost::OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MIDIHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MIDIHostMsg_StartSession, OnStartSession)
    IPC_MESSAGE_HANDLER(MIDIHostMsg_SendData, OnSendData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void MIDIHost::OnStartSession(int client_id) {
  MIDIPortInfoList input_ports;
  MIDIPortInfoList output_ports;

  // Initialize devices and register to receive MIDI data.
  bool success = false;
  if (midi_manager_) {
    success = midi_manager_->StartSession(this);
    if (success) {
      input_ports = midi_manager_->input_ports();
      output_ports = midi_manager_->output_ports();
    }
  }

  Send(new MIDIMsg_SessionStarted(
       client_id,
       success,
       input_ports,
       output_ports));
}

void MIDIHost::OnSendData(int port,
                          const std::vector<uint8>& data,
                          double timestamp) {
  // TODO(crogers): we need to post this to a dedicated thread for
  // sending MIDI. For now, we will not implement the sending of MIDI data.
  NOTIMPLEMENTED();
  // if (midi_manager_)
  //   midi_manager_->SendMIDIData(port, data.data(), data.size(), timestamp);
}

void MIDIHost::ReceiveMIDIData(
    int port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  TRACE_EVENT0("midi", "MIDIHost::ReceiveMIDIData");
  // Send to the renderer.
  std::vector<uint8> v(data, data + length);
  Send(new MIDIMsg_DataReceived(port_index, v, timestamp));
}

}  // namespace content
