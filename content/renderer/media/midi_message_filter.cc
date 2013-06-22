// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_message_filter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

using media::MIDIPortInfoList;
using base::AutoLock;

namespace content {

MIDIMessageFilter::MIDIMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : channel_(NULL),
      io_message_loop_(io_message_loop),
      next_available_id_(0) {
}

MIDIMessageFilter::~MIDIMessageFilter() {}

void MIDIMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!channel_) {
    delete message;
  } else {
    channel_->Send(message);
  }
}

bool MIDIMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MIDIMessageFilter, message)
    IPC_MESSAGE_HANDLER(MIDIMsg_AccessApproved, OnAccessApproved)
    IPC_MESSAGE_HANDLER(MIDIMsg_DataReceived, OnDataReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MIDIMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = channel;
}

void MIDIMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void MIDIMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
}

void MIDIMessageFilter::RequestAccess(
    WebKit::WebMIDIAccessorClient* client, int access) {
  // Generate and keep track of a "client id" which is sent to the browser
  // to ask permission to talk to MIDI hardware.
  // This id is handed back when we receive the answer in OnAccessApproved().
  if (clients_.find(client) == clients_.end()) {
    int client_id = next_available_id_++;
    clients_[client] = client_id;

    io_message_loop_->PostTask(FROM_HERE,
        base::Bind(&MIDIMessageFilter::RequestAccessOnIOThread, this,
                   client_id, access));
  }
}

void MIDIMessageFilter::RequestAccessOnIOThread(int client_id, int access) {
  Send(new MIDIHostMsg_RequestAccess(client_id, access));
}

void MIDIMessageFilter::RemoveClient(WebKit::WebMIDIAccessorClient* client) {
  ClientsMap::iterator i = clients_.find(client);
  if (i != clients_.end())
    clients_.erase(i);
}

// Received from browser.

void MIDIMessageFilter::OnAccessApproved(
    int client_id,
    int access,
    bool success,
    MIDIPortInfoList inputs,
    MIDIPortInfoList outputs) {
  // Handle on the main JS thread.
  ChildThread::current()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&MIDIMessageFilter::HandleAccessApproved, this,
                 client_id, access, success, inputs, outputs));
}

void MIDIMessageFilter::HandleAccessApproved(
    int client_id,
    int access,
    bool success,
    MIDIPortInfoList inputs,
    MIDIPortInfoList outputs) {
  WebKit::WebMIDIAccessorClient* client = GetClientFromId(client_id);
  if (!client)
    return;

  if (success) {
    // Add the client's input and output ports.
    for (size_t i = 0; i < inputs.size(); ++i) {
      client->didAddInputPort(
          UTF8ToUTF16(inputs[i].id),
          UTF8ToUTF16(inputs[i].manufacturer),
          UTF8ToUTF16(inputs[i].name),
          UTF8ToUTF16(inputs[i].version));
    }

    for (size_t i = 0; i < outputs.size(); ++i) {
      client->didAddOutputPort(
          UTF8ToUTF16(outputs[i].id),
          UTF8ToUTF16(outputs[i].manufacturer),
          UTF8ToUTF16(outputs[i].name),
          UTF8ToUTF16(outputs[i].version));
    }
  }

  if (success)
    client->didAllowAccess();
  else
    client->didBlockAccess();
}

WebKit::WebMIDIAccessorClient*
MIDIMessageFilter::GetClientFromId(int client_id) {
  // Iterating like this seems inefficient, but in practice there generally
  // will be very few clients (usually one).  Additionally, this lookup
  // usually happens one time during page load. So the performance hit is
  // negligible.
  for (ClientsMap::iterator i = clients_.begin(); i != clients_.end(); ++i) {
    if ((*i).second == client_id)
      return (*i).first;
  }
  return NULL;
}

void MIDIMessageFilter::OnDataReceived(int port,
                                       const std::vector<uint8>& data,
                                       double timestamp) {
  TRACE_EVENT0("midi", "MIDIMessageFilter::OnDataReceived");

  ChildThread::current()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&MIDIMessageFilter::HandleDataReceived, this,
                 port, data, timestamp));
}

void MIDIMessageFilter::HandleDataReceived(int port,
                                           const std::vector<uint8>& data,
                                           double timestamp) {
  TRACE_EVENT0("midi", "MIDIMessageFilter::HandleDataReceived");

#if defined(OS_ANDROID)
  // TODO(crogers): figure out why data() method does not compile on Android.
  NOTIMPLEMENTED();
#else
  for (ClientsMap::iterator i = clients_.begin(); i != clients_.end(); ++i)
    (*i).first->didReceiveMIDIData(port, data.data(), data.size(), timestamp);
#endif
}

void MIDIMessageFilter::SendMIDIData(int port,
                                     const uint8* data,
                                     size_t length,
                                     double timestamp) {
  // TODO(crogers): we need more work to check the amount of data sent,
  // throttle if necessary, and filter out the SYSEX messages if not
  // approved. For now, we will not implement the sending of MIDI data.
  NOTIMPLEMENTED();
  // std::vector<uint8> v(data, data + length);
  // io_message_loop_->PostTask(FROM_HERE,
  //     base::Bind(&MIDIMessageFilter::SendMIDIDataOnIOThread, this,
  //                port, v, timestamp));
}

void MIDIMessageFilter::SendMIDIDataOnIOThread(int port,
                                               const std::vector<uint8>& data,
                                               double timestamp) {
  // Send to the browser.
  Send(new MIDIHostMsg_SendData(port, data, timestamp));
}

}  // namespace content
