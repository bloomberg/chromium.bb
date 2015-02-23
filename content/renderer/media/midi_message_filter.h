// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_result.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles MIDI messages.
class CONTENT_EXPORT MidiMessageFilter : public IPC::MessageFilter {
 public:
  explicit MidiMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Each client registers for MIDI access here.
  // If permission is granted, then the client's
  void AddClient(blink::WebMIDIAccessorClient* client);
  void RemoveClient(blink::WebMIDIAccessorClient* client);

  // A client will only be able to call this method if it has a suitable
  // output port (from addOutputPort()).
  void SendMidiData(uint32 port,
                    const uint8* data,
                    size_t length,
                    double timestamp);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

  static blink::WebMIDIAccessorClient::MIDIPortState ToBlinkState(
      media::MidiPortState state) {
    return static_cast<blink::WebMIDIAccessorClient::MIDIPortState>(state);
  }

 protected:
  ~MidiMessageFilter() override;

 private:
  void StartSessionOnIOThread();

  void SendMidiDataOnIOThread(uint32 port,
                              const std::vector<uint8>& data,
                              double timestamp);

  void EndSessionOnIOThread();

  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_message_loop|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Called when the browser process has approved (or denied) access to
  // MIDI hardware.
  void OnSessionStarted(media::MidiResult result);

  // These functions are called in 2 cases:
  //  (1) Just before calling |OnSessionStarted|, to notify the recipient about
  //      existing ports.
  //  (2) To notify the recipient that a new device was connected and that new
  //      ports have been created.
  void OnAddInputPort(media::MidiPortInfo info);
  void OnAddOutputPort(media::MidiPortInfo info);

  // These functions are called to notify the recipient that a device that is
  // notified via OnAddInputPort() or OnAddOutputPort() gets disconnected, or
  // connected again.
  void OnSetInputPortState(uint32 port, media::MidiPortState state);
  void OnSetOutputPortState(uint32 port, media::MidiPortState state);

  // Called when the browser process has sent MIDI data containing one or
  // more messages.
  void OnDataReceived(uint32 port,
                      const std::vector<uint8>& data,
                      double timestamp);

  // From time-to-time, the browser incrementally informs us of how many bytes
  // it has successfully sent. This is part of our throttling process to avoid
  // sending too much data before knowing how much has already been sent.
  void OnAcknowledgeSentData(size_t bytes_sent);

  // Following methods, Handle*, run on |main_message_loop_|.
  void HandleClientAdded(media::MidiResult result);

  void HandleAddInputPort(media::MidiPortInfo info);
  void HandleAddOutputPort(media::MidiPortInfo info);
  void HandleSetInputPortState(uint32 port, media::MidiPortState state);
  void HandleSetOutputPortState(uint32 port, media::MidiPortState state);

  void HandleDataReceived(uint32 port,
                          const std::vector<uint8>& data,
                          double timestamp);

  void HandleAckknowledgeSentData(size_t bytes_sent);

  // IPC sender for Send(); must only be accessed on |io_message_loop_|.
  IPC::Sender* sender_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Main thread's message loop.
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  /*
   * Notice: Following members are designed to be accessed only on
   * |main_message_loop_|.
   */
  // Keeps track of all MIDI clients.
  typedef std::set<blink::WebMIDIAccessorClient*> ClientsSet;
  ClientsSet clients_;

  // Represents clients that are waiting for a session being open.
  typedef std::vector<blink::WebMIDIAccessorClient*> ClientsQueue;
  ClientsQueue clients_waiting_session_queue_;

  // Represents a result on starting a session. Can be accessed only on
  media::MidiResult session_result_;

  // Holds MidiPortInfoList for input ports and output ports.
  media::MidiPortInfoList inputs_;
  media::MidiPortInfoList outputs_;

  size_t unacknowledged_bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(MidiMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
