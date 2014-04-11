// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_H_
#define MEDIA_MIDI_MIDI_MANAGER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_result.h"

namespace media {

// A MidiManagerClient registers with the MidiManager to receive MIDI data.
// See MidiManager::RequestAccess() and MidiManager::ReleaseAccess()
// for details.
class MEDIA_EXPORT MidiManagerClient {
 public:
  virtual ~MidiManagerClient() {}

  // CompleteStartSession() is called when platform dependent preparation is
  // finished.
  virtual void CompleteStartSession(int client_id, MidiResult result) = 0;

  // ReceiveMidiData() is called when MIDI data has been received from the
  // MIDI system.
  // |port_index| represents the specific input port from input_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time the data was received, in seconds.
  virtual void ReceiveMidiData(uint32 port_index,
                               const uint8* data,
                               size_t length,
                               double timestamp) = 0;

  // AccumulateMidiBytesSent() is called to acknowledge when bytes have
  // successfully been sent to the hardware.
  // This happens as a result of the client having previously called
  // MidiManager::DispatchSendMidiData().
  virtual void AccumulateMidiBytesSent(size_t n) = 0;
};

// Manages access to all MIDI hardware.
class MEDIA_EXPORT MidiManager {
 public:
  static MidiManager* Create();

  MidiManager();
  virtual ~MidiManager();

  // A client calls StartSession() to receive and send MIDI data.
  // If the session is ready to start, the MIDI system is lazily initialized
  // and the client is registered to receive MIDI data.
  // CompleteStartSession() is called with MIDI_OK if the session is started.
  // Otherwise CompleteStartSession() is called with proper MidiResult code.
  void StartSession(MidiManagerClient* client, int client_id);

  // A client calls EndSession() to stop receiving MIDI data.
  void EndSession(MidiManagerClient* client);

  // DispatchSendMidiData() is called when MIDI data should be sent to the MIDI
  // system.
  // This method is supposed to return immediately and should not block.
  // |port_index| represents the specific output port from output_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time to send the data, in seconds. A value of 0
  // means send "now" or as soon as possible.
  // The default implementation is for unsupported platforms.
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp);

  // input_ports() is a list of MIDI ports for receiving MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MidiPortInfoList& input_ports() { return input_ports_; }

  // output_ports() is a list of MIDI ports for sending MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MidiPortInfoList& output_ports() { return output_ports_; }

 protected:
  // Initializes the MIDI system, returning |true| on success.
  // The default implementation is for unsupported platforms.
  virtual MidiResult Initialize();

  void AddInputPort(const MidiPortInfo& info);
  void AddOutputPort(const MidiPortInfo& info);

  // Dispatches to all clients.
  void ReceiveMidiData(uint32 port_index,
                       const uint8* data,
                       size_t length,
                       double timestamp);

  bool initialized_;
  MidiResult result_;

  // Keeps track of all clients who wish to receive MIDI data.
  typedef std::set<MidiManagerClient*> ClientList;
  ClientList clients_;

  // Protects access to our clients.
  base::Lock clients_lock_;

  MidiPortInfoList input_ports_;
  MidiPortInfoList output_ports_;

  DISALLOW_COPY_AND_ASSIGN(MidiManager);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_H_
