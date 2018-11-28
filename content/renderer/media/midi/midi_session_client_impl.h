// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_MIDI_SESSION_CLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MIDI_MIDI_SESSION_CLIENT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/midi/midi_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/webmidi/web_midi_accessor_client.h"

namespace content {

// Created on render thread, and hosts multiple clients running on multiple
// frames. All operations are carried out on |main_task_runner_|.
class CONTENT_EXPORT MidiSessionClientImpl
    : public midi::mojom::MidiSessionClient {
 public:
  explicit MidiSessionClientImpl();
  ~MidiSessionClientImpl() override;

  // midi::mojom::MidiSessionClient implementation.
  // All of the following methods are run on the main thread.
  void AddInputPort(midi::mojom::PortInfoPtr info) override;
  void AddOutputPort(midi::mojom::PortInfoPtr info) override;
  void SetInputPortState(uint32_t port, midi::mojom::PortState state) override;
  void SetOutputPortState(uint32_t port, midi::mojom::PortState state) override;
  void SessionStarted(midi::mojom::Result result) override;
  void AcknowledgeSentData(uint32_t bytes) override;
  void DataReceived(uint32_t port,
                    const std::vector<uint8_t>& data,
                    base::TimeTicks timestamp) override;

  // Each client registers for MIDI access here.
  void AddClient(blink::WebMIDIAccessorClient* client);
  void RemoveClient(blink::WebMIDIAccessorClient* client);

  // Called to clean up if browser terminates session.
  void RemoveClients();

  // A client will only be able to call this method if it has a suitable
  // output port (from AddOutputPort()).
  void SendMidiData(uint32_t port,
                    const uint8_t* data,
                    size_t length,
                    base::TimeTicks timestamp);

 private:
  midi::mojom::MidiSessionProvider& GetMidiSessionProvider();
  midi::mojom::MidiSession& GetMidiSession();

  // Keeps track of all MIDI clients. This should be std::set so that various
  // for-loops work correctly. To change the type, make sure that the new type
  // is safe to modify the container inside for-loops.
  typedef std::set<blink::WebMIDIAccessorClient*> ClientsSet;
  ClientsSet clients_;

  // Represents clients that are waiting for a session being open.
  // Note: std::vector is not safe to invoke callbacks inside iterator based
  // for-loops.
  typedef std::vector<blink::WebMIDIAccessorClient*> ClientsQueue;
  ClientsQueue clients_waiting_session_queue_;

  // Represents a result on starting a session.
  midi::mojom::Result session_result_;

  // Holds MidiPortInfoList for input ports and output ports.
  std::vector<midi::mojom::PortInfo> inputs_;
  std::vector<midi::mojom::PortInfo> outputs_;

  size_t unacknowledged_bytes_sent_;

  midi::mojom::MidiSessionProviderPtr midi_session_provider_;
  midi::mojom::MidiSessionPtr midi_session_;
  mojo::Binding<midi::mojom::MidiSessionClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MidiSessionClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_MIDI_SESSION_CLIENT_IMPL_H_
