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
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, UdevEscape);

  class CardInfo {
   public:
    CardInfo(const MidiManagerAlsa* outer,
             const std::string& alsa_name, const std::string& alsa_longname,
             const std::string& alsa_driver, int card_index);
    ~CardInfo();

    const std::string alsa_name() const;
    const std::string manufacturer() const;
    const std::string alsa_driver() const;
    const std::string udev_id_path() const;
    const std::string udev_id_id() const;

   private:
    FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);
    FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, UdevEscape);

    // Extracts the manufacturer using heuristics and a variety of sources.
    static std::string ExtractManufacturerString(
        const std::string& udev_id_vendor_enc,
        const std::string& udev_id_vendor_id,
        const std::string& udev_id_vendor_from_database,
        const std::string& alsa_name,
        const std::string& alsa_longname);

    // TODO(agoode): Move this into a common place. Maybe device/udev_linux?
    // Decodes just \xXX in strings.
    static std::string UnescapeUdev(const std::string& s);

    std::string alsa_name_;
    std::string manufacturer_;
    std::string alsa_driver_;
    std::string udev_id_path_;
    std::string udev_id_id_;
  };

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(uint32 port_index,
                    const std::vector<uint8>& data);

  void EventReset();
  void EventLoop();

  // Alsa seq handles.
  snd_seq_t* in_client_;
  snd_seq_t* out_client_;
  int out_client_id_;

  // One input port, many output ports.
  int in_port_;
  std::vector<int> out_ports_;

  // Mapping from Alsa client:port to our index.
  typedef std::map<int, uint32> SourceMap;
  SourceMap source_map_;

  // Alsa event <-> MIDI coders.
  snd_midi_event_t* decoder_;
  typedef std::vector<snd_midi_event_t*> EncoderList;
  EncoderList encoders_;

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
