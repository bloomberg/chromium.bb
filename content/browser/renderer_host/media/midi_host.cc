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

// The total number of bytes which we're allowed to send to the OS
// before knowing that they have been successfully sent.
static const size_t kMaxInFlightBytes = 10 * 1024 * 1024;  // 10 MB.

// We keep track of the number of bytes successfully sent to
// the hardware.  Every once in a while we report back to the renderer
// the number of bytes sent since the last report. This threshold determines
// how many bytes will be sent before reporting back to the renderer.
static const size_t kAcknowledgementThresholdBytes = 1024 * 1024;  // 1 MB.

static const uint8 kSysExMessage = 0xf0;

namespace content {

MIDIHost::MIDIHost(media::MIDIManager* midi_manager)
    : midi_manager_(midi_manager),
      sent_bytes_in_flight_(0),
      bytes_sent_since_last_acknowledgement_(0) {
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
  if (!midi_manager_)
    return;

  base::AutoLock auto_lock(in_flight_lock_);

  // Sanity check that we won't send too much.
  if (sent_bytes_in_flight_ > kMaxInFlightBytes ||
      data.size() > kMaxInFlightBytes ||
      data.size() + sent_bytes_in_flight_ > kMaxInFlightBytes)
    return;

  // For now disallow all System Exclusive messages even if we
  // have permission.
  // TODO(toyoshim): allow System Exclusive if browser has granted
  // this client access.  We'll likely need to pass a GURL
  // here to compare against our permissions.
  if (data.size() > 0 && data[0] >= kSysExMessage)
      return;

#if defined(OS_ANDROID)
  // TODO(toyoshim): figure out why data() method does not compile on Android.
  NOTIMPLEMENTED();
#else
  midi_manager_->DispatchSendMIDIData(
      this,
      port,
      data.data(),
      data.size(),
      timestamp);
#endif

  sent_bytes_in_flight_ += data.size();
}

void MIDIHost::ReceiveMIDIData(
    int port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  TRACE_EVENT0("midi", "MIDIHost::ReceiveMIDIData");

  // For now disallow all System Exclusive messages even if we
  // have permission.
  // TODO(toyoshim): allow System Exclusive if browser has granted
  // this client access.  We'll likely need to pass a GURL
  // here to compare against our permissions.
  if (length > 0 && data[0] >= kSysExMessage)
      return;

  // Send to the renderer.
  std::vector<uint8> v(data, data + length);
  Send(new MIDIMsg_DataReceived(port_index, v, timestamp));
}

void MIDIHost::AccumulateMIDIBytesSent(size_t n) {
  {
    base::AutoLock auto_lock(in_flight_lock_);
    if (n <= sent_bytes_in_flight_)
      sent_bytes_in_flight_ -= n;
  }

  if (bytes_sent_since_last_acknowledgement_ + n >=
      bytes_sent_since_last_acknowledgement_)
    bytes_sent_since_last_acknowledgement_ += n;

  if (bytes_sent_since_last_acknowledgement_ >=
      kAcknowledgementThresholdBytes) {
    Send(new MIDIMsg_AcknowledgeSentData(
        bytes_sent_since_last_acknowledgement_));
    bytes_sent_since_last_acknowledgement_ = 0;
  }
}

}  // namespace content
