// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_MAC_H_
#define MEDIA_MIDI_MIDI_MANAGER_MAC_H_

#include <CoreMIDI/MIDIServices.h>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_port_info.h"

namespace media {

class MEDIA_EXPORT MIDIManagerMac : public MIDIManager {
 public:
  MIDIManagerMac();
  virtual ~MIDIManagerMac();

  // MIDIManager implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void SendMIDIData(MIDIManagerClient* client,
                            int port_index,
                            const uint8* data,
                            size_t length,
                            double timestamp) OVERRIDE;

 private:
  // CoreMIDI callback for MIDI data.
  // Each callback can contain multiple packets, each of which can contain
  // multiple MIDI messages.
  static void ReadMidiDispatch(
      const MIDIPacketList *pktlist,
      void *read_proc_refcon,
      void *src_conn_refcon);
  virtual void ReadMidi(MIDIEndpointRef source, const MIDIPacketList *pktlist);

  // Helper
  static media::MIDIPortInfo GetPortInfoFromEndpoint(MIDIEndpointRef endpoint);
  static double MIDITimeStampToSeconds(MIDITimeStamp timestamp);
  static MIDITimeStamp SecondsToMIDITimeStamp(double seconds);

  // CoreMIDI
  MIDIClientRef midi_client_;
  MIDIPortRef coremidi_input_;
  MIDIPortRef coremidi_output_;

  enum{ kMaxPacketListSize = 512 };
  char midi_buffer_[kMaxPacketListSize];
  MIDIPacketList* packet_list_;
  MIDIPacket* midi_packet_;

  typedef std::map<MIDIEndpointRef, int> SourceMap;

  // Keeps track of the index (0-based) for each of our sources.
  SourceMap source_map_;

  // Keeps track of all destinations.
  std::vector<MIDIEndpointRef> destinations_;

  DISALLOW_COPY_AND_ASSIGN(MIDIManagerMac);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_MAC_H_
