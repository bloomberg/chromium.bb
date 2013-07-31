// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_H_
#define MEDIA_MIDI_MIDI_MANAGER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "media/midi/midi_port_info.h"

namespace base {
class Thread;
}

namespace media {

// A MIDIManagerClient registers with the MIDIManager to receive MIDI data.
// See MIDIManager::RequestAccess() and MIDIManager::ReleaseAccess()
// for details.
class MEDIA_EXPORT MIDIManagerClient {
 public:
   virtual ~MIDIManagerClient() {}

  // ReceiveMIDIData() is called when MIDI data has been received from the
  // MIDI system.
  // |port_index| represents the specific input port from input_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time the data was received, in seconds.
  virtual void ReceiveMIDIData(int port_index,
                               const uint8* data,
                               size_t length,
                               double timestamp) = 0;

  // AccumulateMIDIBytesSent() is called to acknowledge when bytes have
  // successfully been sent to the hardware.
  // This happens as a result of the client having previously called
  // MIDIManager::DispatchSendMIDIData().
  virtual void AccumulateMIDIBytesSent(size_t n) = 0;
};

// Manages access to all MIDI hardware.
class MEDIA_EXPORT MIDIManager {
 public:
  static MIDIManager* Create();

  MIDIManager();
  virtual ~MIDIManager();

  // A client calls StartSession() to receive and send MIDI data.
  // If the session is ready to start, the MIDI system is lazily initialized
  // and the client is registered to receive MIDI data.
  // Returns |true| if the session succeeds to start.
  bool StartSession(MIDIManagerClient* client);

  // A client calls ReleaseSession() to stop receiving MIDI data.
  void EndSession(MIDIManagerClient* client);

  // DispatchSendMIDIData() schedules one or more messages to be sent
  // at the given time on a dedicated thread.
  // |port_index| represents the specific output port from output_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time to send the data, in seconds. A value of 0
  // means send "now" or as soon as possible.
  void DispatchSendMIDIData(MIDIManagerClient* client,
                            int port_index,
                            const uint8* data,
                            size_t length,
                            double timestamp);

  // input_ports() is a list of MIDI ports for receiving MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MIDIPortInfoList& input_ports() { return input_ports_; }

  // output_ports() is a list of MIDI ports for sending MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MIDIPortInfoList& output_ports() { return output_ports_; }

 protected:
  // Initializes the MIDI system, returning |true| on success.
  virtual bool Initialize() = 0;

  // Implements the platform-specific details of sending MIDI data.
  virtual void SendMIDIData(MIDIManagerClient* client,
                            int port_index,
                            const uint8* data,
                            size_t length,
                            double timestamp) = 0;

  void AddInputPort(const MIDIPortInfo& info);
  void AddOutputPort(const MIDIPortInfo& info);

  // Dispatches to all clients.
  void ReceiveMIDIData(
      int port_index,
      const uint8* data,
      size_t length,
      double timestamp);

  bool initialized_;

  // Keeps track of all clients who wish to receive MIDI data.
  typedef std::set<MIDIManagerClient*> ClientList;
  ClientList clients_;

  // Protects access to our clients.
  base::Lock clients_lock_;

  MIDIPortInfoList input_ports_;
  MIDIPortInfoList output_ports_;

  // |send_thread_| is used to send MIDI data by calling the platform-specific
  // API.
  scoped_ptr<base::Thread> send_thread_;
  scoped_refptr<base::MessageLoopProxy> send_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MIDIManager);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_H_
