// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <iostream>
#include <string>

#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include <CoreAudio/HostTime.h>

using base::IntToString;
using base::SysCFStringRefToUTF8;
using std::string;

// NB: System MIDI types are pointer types in 32-bit and integer types in
// 64-bit. Therefore, the initialization is the simplest one that satisfies both
// (if possible).

namespace media {

MIDIManager* MIDIManager::Create() {
  return new MIDIManagerMac();
}

MIDIManagerMac::MIDIManagerMac()
    : midi_client_(0),
      coremidi_input_(0),
      coremidi_output_(0),
      packet_list_(NULL),
      midi_packet_(NULL) {
}

bool MIDIManagerMac::Initialize() {
  TRACE_EVENT0("midi", "MIDIManagerMac::Initialize");

  // CoreMIDI registration.
  midi_client_ = 0;
  OSStatus result = MIDIClientCreate(
      CFSTR("Google Chrome"),
      NULL,
      NULL,
      &midi_client_);

  if (result != noErr)
    return false;

  coremidi_input_ = 0;

  // Create input and output port.
  result = MIDIInputPortCreate(
      midi_client_,
      CFSTR("MIDI Input"),
      ReadMidiDispatch,
      this,
      &coremidi_input_);
  if (result != noErr)
    return false;

  result = MIDIOutputPortCreate(
      midi_client_,
      CFSTR("MIDI Output"),
      &coremidi_output_);
  if (result != noErr)
    return false;

  int destination_count = MIDIGetNumberOfDestinations();
  destinations_.reserve(destination_count);

  for (int i = 0; i < destination_count ; i++) {
    MIDIEndpointRef destination = MIDIGetDestination(i);

    // Keep track of all destinations (known as outputs by the Web MIDI API).
    // Cache to avoid any possible overhead in calling MIDIGetDestination().
    destinations_[i] = destination;

    MIDIPortInfo info = GetPortInfoFromEndpoint(destination);
    AddOutputPort(info);
  }

  // Open connections from all sources.
  int source_count = MIDIGetNumberOfSources();

  for (int i = 0; i < source_count; ++i)  {
    // Receive from all sources.
    MIDIEndpointRef src = MIDIGetSource(i);
    MIDIPortConnectSource(coremidi_input_, src, reinterpret_cast<void*>(src));

    // Keep track of all sources (known as inputs in Web MIDI API terminology).
    source_map_[src] = i;

    MIDIPortInfo info = GetPortInfoFromEndpoint(src);
    AddInputPort(info);
  }

  // TODO(crogers): Fix the memory management here!
  packet_list_ = reinterpret_cast<MIDIPacketList*>(midi_buffer_);
  midi_packet_ = MIDIPacketListInit(packet_list_);

  return true;
}

MIDIManagerMac::~MIDIManagerMac() {
  if (coremidi_input_)
    MIDIPortDispose(coremidi_input_);
  if (coremidi_output_)
    MIDIPortDispose(coremidi_output_);
}

void MIDIManagerMac::ReadMidiDispatch(const MIDIPacketList* packet_list,
                                      void* read_proc_refcon,
                                      void* src_conn_refcon) {
  MIDIManagerMac* manager = static_cast<MIDIManagerMac*>(read_proc_refcon);
#if __LP64__
  MIDIEndpointRef source = reinterpret_cast<uintptr_t>(src_conn_refcon);
#else
  MIDIEndpointRef source = static_cast<MIDIEndpointRef>(src_conn_refcon);
#endif

  // Dispatch to class method.
  manager->ReadMidi(source, packet_list);
}

void MIDIManagerMac::ReadMidi(MIDIEndpointRef source,
                              const MIDIPacketList* packet_list) {
  // Lookup the port index based on the source.
  SourceMap::iterator j = source_map_.find(source);
  if (j == source_map_.end())
    return;
  int port_index = source_map_[source];

  // Go through each packet and process separately.
  for(size_t i = 0; i < packet_list->numPackets; i++) {
    // Each packet contains MIDI data for one or more messages (like note-on).
    const MIDIPacket &packet = packet_list->packet[i];
    double timestamp_seconds = MIDITimeStampToSeconds(packet.timeStamp);

    ReceiveMIDIData(
        port_index,
        packet.data,
        packet.length,
        timestamp_seconds);
  }
}

void MIDIManagerMac::SendMIDIData(MIDIManagerClient* client,
                                  int port_index,
                                  const uint8* data,
                                  size_t length,
                                  double timestamp) {
  // System Exclusive has already been filtered.
  MIDITimeStamp coremidi_timestamp = SecondsToMIDITimeStamp(timestamp);

  midi_packet_ = MIDIPacketListAdd(
      packet_list_,
      kMaxPacketListSize,
      midi_packet_,
      coremidi_timestamp,
      length,
      data);

  // Lookup the destination based on the port index.
  // TODO(crogers): re-factor |port_index| to use unsigned
  // to avoid the need for this check.
  if (port_index < 0 ||
      static_cast<size_t>(port_index) >= destinations_.size())
    return;

  MIDIEndpointRef destination = destinations_[port_index];

  MIDISend(coremidi_output_, destination, packet_list_);

  // Re-initialize for next time.
  midi_packet_ = MIDIPacketListInit(packet_list_);

  client->AccumulateMIDIBytesSent(length);
}

MIDIPortInfo MIDIManagerMac::GetPortInfoFromEndpoint(
    MIDIEndpointRef endpoint) {
  SInt32 id_number = 0;
  MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &id_number);
  string id = IntToString(id_number);

  CFStringRef manufacturer_ref = NULL;
  MIDIObjectGetStringProperty(
      endpoint, kMIDIPropertyManufacturer, &manufacturer_ref);
  string manufacturer = SysCFStringRefToUTF8(manufacturer_ref);

  CFStringRef name_ref = NULL;
  MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name_ref);
  string name = SysCFStringRefToUTF8(name_ref);

  SInt32 version_number = 0;
  MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyDriverVersion, &version_number);
  string version = IntToString(version_number);

  return MIDIPortInfo(id, manufacturer, name, version);
}

double MIDIManagerMac::MIDITimeStampToSeconds(MIDITimeStamp timestamp) {
  UInt64 nanoseconds = AudioConvertHostTimeToNanos(timestamp);
  return static_cast<double>(nanoseconds) / 1.0e9;
}

MIDITimeStamp MIDIManagerMac::SecondsToMIDITimeStamp(double seconds) {
  UInt64 nanos = UInt64(seconds * 1.0e9);
  return AudioConvertNanosToHostTime(nanos);
}

}  // namespace media
