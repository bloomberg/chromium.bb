// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
#define MEDIA_MIDI_MIDI_MANAGER_ALSA_H_

#include <alsa/asoundlib.h>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "media/midi/midi_manager.h"

#if defined(USE_UDEV)
#include "device/udev_linux/scoped_udev.h"
#endif  // defined(USE_UDEV)

namespace media {

class MEDIA_EXPORT MidiManagerAlsa : public MidiManager {
 public:
  MidiManagerAlsa();
  ~MidiManagerAlsa() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32 port_index,
                            const std::vector<uint8>& data,
                            double timestamp) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, JSONPortMetadata);

  class AlsaRawmidi {
   public:
    AlsaRawmidi(const MidiManagerAlsa* outer,
                const std::string& alsa_name,
                const std::string& alsa_longname,
                const std::string& alsa_driver,
                int card_index);
    ~AlsaRawmidi();

    const std::string alsa_name() const;
    const std::string alsa_longname() const;
    const std::string manufacturer() const;
    const std::string alsa_driver() const;
    const std::string path() const;
    const std::string bus() const;
    const std::string vendor_id() const;
    const std::string id() const;

   private:
    FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);

    // Extracts the manufacturer using heuristics and a variety of sources.
    static std::string ExtractManufacturerString(
        const std::string& udev_id_vendor,
        const std::string& udev_id_vendor_id,
        const std::string& udev_id_vendor_from_database,
        const std::string& alsa_name,
        const std::string& alsa_longname);

    std::string alsa_name_;
    std::string alsa_longname_;
    std::string manufacturer_;
    std::string alsa_driver_;
    std::string path_;
    std::string bus_;
    std::string vendor_id_;
    std::string model_id_;
    std::string usb_interface_num_;

    DISALLOW_COPY_AND_ASSIGN(AlsaRawmidi);
  };

  class AlsaPortMetadata {
   public:
    enum class Type { kInput, kOutput };

    AlsaPortMetadata(const std::string& path,
                     const std::string& bus,
                     const std::string& id,
                     const snd_seq_addr_t* address,
                     const std::string& client_name,
                     const std::string& port_name,
                     const std::string& card_name,
                     const std::string& card_longname,
                     Type type);
    ~AlsaPortMetadata();

    // Gets a Value representation of this object, suitable for serialization.
    scoped_ptr<base::Value> Value() const;

    // Gets a string version of Value in JSON format.
    std::string JSONValue() const;

    // Gets an opaque identifier for this object, suitable for using as the id
    // field in MidiPort.id on the web. Note that this string does not store
    // the full state.
    std::string OpaqueKey() const;

   private:
    FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, JSONPortMetadata);

    const std::string path_;
    const std::string bus_;
    const std::string id_;
    const int client_addr_;
    const int port_addr_;
    const std::string client_name_;
    const std::string port_name_;
    const std::string card_name_;
    const std::string card_longname_;
    const Type type_;

    DISALLOW_COPY_AND_ASSIGN(AlsaPortMetadata);
  };

  // Returns an ordered vector of all the rawmidi devices on the system.
  ScopedVector<AlsaRawmidi> AllAlsaRawmidis();

  // Enumerate all the ports for initial setup.
  void EnumeratePorts();

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(uint32 port_index,
                    const std::vector<uint8>& data);

  void ScheduleEventLoop();
  void EventLoop();
  void ProcessSingleEvent(snd_seq_event_t* event, double timestamp);

  // ALSA seq handles.
  snd_seq_t* in_client_;
  snd_seq_t* out_client_;
  int out_client_id_;

  // One input port, many output ports.
  int in_port_;
  std::vector<int> out_ports_;

  // Mapping from ALSA client:port to our index.
  typedef std::map<int, uint32> SourceMap;
  SourceMap source_map_;

  // ALSA event <-> MIDI coders.
  snd_midi_event_t* decoder_;

  // udev, for querying hardware devices.
#if defined(USE_UDEV)
  device::ScopedUdevPtr udev_;
#endif  // defined(USE_UDEV)

  base::Thread send_thread_;
  base::Thread event_thread_;

  bool event_thread_shutdown_; // guarded by shutdown_lock_
  base::Lock shutdown_lock_; // guards event_thread_shutdown_

  DISALLOW_COPY_AND_ASSIGN(MidiManagerAlsa);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
