// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include <alsa/asoundlib.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "media/midi/midi_port_info.h"

namespace media {

class MidiManagerAlsa::MidiDeviceInfo
    : public base::RefCounted<MidiDeviceInfo> {
 public:
  MidiDeviceInfo(MidiManagerAlsa* manager,
                 const std::string& bus_id,
                 snd_ctl_card_info_t* card,
                 const snd_rawmidi_info_t* midi,
                 int device) {
    opened_ = !snd_rawmidi_open(&midi_in_, &midi_out_, bus_id.c_str(), 0);
    if (!opened_)
      return;

    const std::string id = base::StringPrintf("%s:%d", bus_id.c_str(), device);
    const std::string name = snd_rawmidi_info_get_name(midi);
    // We assume that card longname is in the format of
    // "<manufacturer> <name> at <bus>". Otherwise, we give up to detect
    // a manufacturer name here.
    std::string manufacturer;
    const std::string card_name = snd_ctl_card_info_get_longname(card);
    size_t name_index = card_name.find(name);
    if (std::string::npos != name_index)
      manufacturer = card_name.substr(0, name_index - 1);
    const std::string version =
        base::StringPrintf("%s / ALSA library version %d.%d.%d",
                           snd_ctl_card_info_get_driver(card),
                           SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR);
    port_info_ = MidiPortInfo(id, manufacturer, name, version);
  }

  void Send(MidiManagerClient* client, const std::vector<uint8>& data) {
    ssize_t result = snd_rawmidi_write(
        midi_out_, reinterpret_cast<const void*>(&data[0]), data.size());
    if (static_cast<size_t>(result) != data.size()) {
      // TODO(toyoshim): Disconnect and reopen the device.
      LOG(ERROR) << "snd_rawmidi_write fails: " << strerror(-result);
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MidiManagerClient::AccumulateMidiBytesSent,
                   base::Unretained(client), data.size()));
  }

  const MidiPortInfo& GetMidiPortInfo() const { return port_info_; }
  bool IsOpened() const { return opened_; }

 private:
  friend class base::RefCounted<MidiDeviceInfo>;
  virtual ~MidiDeviceInfo() {
    if (opened_) {
      snd_rawmidi_close(midi_in_);
      snd_rawmidi_close(midi_out_);
    }
  }

  bool opened_;
  MidiPortInfo port_info_;
  snd_rawmidi_t* midi_in_;
  snd_rawmidi_t* midi_out_;

  DISALLOW_COPY_AND_ASSIGN(MidiDeviceInfo);
};

MidiManagerAlsa::MidiManagerAlsa()
    : send_thread_("MidiSendThread") {
}

bool MidiManagerAlsa::Initialize() {
  // TODO(toyoshim): Make Initialize() asynchronous.
  // See http://crbug.com/339746.
  TRACE_EVENT0("midi", "MidiManagerMac::Initialize");

  // Enumerate only hardware MIDI devices because software MIDIs running in
  // the browser process is not secure.
  snd_ctl_card_info_t* card;
  snd_rawmidi_info_t* midi_out;
  snd_rawmidi_info_t* midi_in;
  snd_ctl_card_info_alloca(&card);
  snd_rawmidi_info_alloca(&midi_out);
  snd_rawmidi_info_alloca(&midi_in);
  for (int index = -1; !snd_card_next(&index) && index >= 0; ) {
    const std::string id = base::StringPrintf("hw:CARD=%i", index);
    snd_ctl_t* handle;
    int err = snd_ctl_open(&handle, id.c_str(), 0);
    if (err != 0) {
      DLOG(ERROR) << "snd_ctl_open fails: " << snd_strerror(err);
      continue;
    }
    err = snd_ctl_card_info(handle, card);
    if (err != 0) {
      DLOG(ERROR) << "snd_ctl_card_info fails: " << snd_strerror(err);
      snd_ctl_close(handle);
      continue;
    }
    for (int device = -1;
        !snd_ctl_rawmidi_next_device(handle, &device) && device >= 0; ) {
      bool output;
      bool input;
      snd_rawmidi_info_set_device(midi_out, device);
      snd_rawmidi_info_set_subdevice(midi_out, 0);
      snd_rawmidi_info_set_stream(midi_out, SND_RAWMIDI_STREAM_OUTPUT);
      output = snd_ctl_rawmidi_info(handle, midi_out) == 0;
      snd_rawmidi_info_set_device(midi_in, device);
      snd_rawmidi_info_set_subdevice(midi_in, 0);
      snd_rawmidi_info_set_stream(midi_in, SND_RAWMIDI_STREAM_INPUT);
      input = snd_ctl_rawmidi_info(handle, midi_in) == 0;
      if (!output && !input)
        continue;
      scoped_refptr<MidiDeviceInfo> port = new MidiDeviceInfo(
          this, id, card, output ? midi_out : midi_in, device);
      if (!port->IsOpened()) {
        DLOG(ERROR) << "MidiDeviceInfo open fails";
        continue;
      }
      if (input) {
        in_devices_.push_back(port);
        AddInputPort(port->GetMidiPortInfo());
      }
      if (output) {
        out_devices_.push_back(port);
        AddOutputPort(port->GetMidiPortInfo());
      }
    }
    snd_ctl_close(handle);
  }
  return true;
}

MidiManagerAlsa::~MidiManagerAlsa() {
  send_thread_.Stop();
}

void MidiManagerAlsa::DispatchSendMidiData(MidiManagerClient* client,
                                           uint32 port_index,
                                           const std::vector<uint8>& data,
                                           double timestamp) {
  if (out_devices_.size() <= port_index)
    return;

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
            timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }

  if (!send_thread_.IsRunning())
    send_thread_.Start();

  scoped_refptr<MidiDeviceInfo> device = out_devices_[port_index];
  send_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MidiDeviceInfo::Send, device, client, data),
      delay);
}

MidiManager* MidiManager::Create() {
  return new MidiManagerAlsa();
}

}  // namespace media
