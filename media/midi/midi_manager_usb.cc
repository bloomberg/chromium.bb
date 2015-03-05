// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_usb.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "media/midi/usb_midi_descriptor_parser.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_input_stream.h"
#include "media/midi/usb_midi_jack.h"
#include "media/midi/usb_midi_output_stream.h"

namespace media {

MidiManagerUsb::MidiManagerUsb(scoped_ptr<UsbMidiDevice::Factory> factory)
    : device_factory_(factory.Pass()) {
}

MidiManagerUsb::~MidiManagerUsb() {
}

void MidiManagerUsb::StartInitialization() {
  Initialize(
      base::Bind(&MidiManager::CompleteInitialization, base::Unretained(this)));
}

void MidiManagerUsb::Initialize(
    base::Callback<void(MidiResult result)> callback) {
  initialize_callback_ = callback;
  // This is safe because EnumerateDevices cancels the operation on destruction.
  device_factory_->EnumerateDevices(
      this,
      base::Bind(&MidiManagerUsb::OnEnumerateDevicesDone,
                 base::Unretained(this)));
}

void MidiManagerUsb::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8>& data,
                                          double timestamp) {
  if (port_index >= output_streams_.size()) {
    // |port_index| is provided by a renderer so we can't believe that it is
    // in the valid range.
    return;
  }
  output_streams_[port_index]->Send(data);
  client->AccumulateMidiBytesSent(data.size());
}

void MidiManagerUsb::ReceiveUsbMidiData(UsbMidiDevice* device,
                                        int endpoint_number,
                                        const uint8* data,
                                        size_t size,
                                        base::TimeTicks time) {
  if (!input_stream_)
    return;
  input_stream_->OnReceivedData(device,
                                endpoint_number,
                                data,
                                size,
                                time);
}

void MidiManagerUsb::OnDeviceAttached(scoped_ptr<UsbMidiDevice> device) {
  int device_id = devices_.size();
  devices_.push_back(device.release());
  AddPorts(devices_.back(), device_id);
}

void MidiManagerUsb::OnDeviceDetached(size_t index) {
  if (index >= devices_.size()) {
    return;
  }
  UsbMidiDevice* device = devices_[index];
  for (size_t i = 0; i < output_streams_.size(); ++i) {
    if (output_streams_[i]->jack().device == device) {
      SetOutputPortState(i, MIDI_PORT_DISCONNECTED);
    }
  }
  const std::vector<UsbMidiJack>& input_jacks = input_stream_->jacks();
  for (size_t i = 0; i < input_jacks.size(); ++i) {
    if (input_jacks[i].device == device) {
      SetInputPortState(i, MIDI_PORT_DISCONNECTED);
    }
  }
}

void MidiManagerUsb::OnReceivedData(size_t jack_index,
                                    const uint8* data,
                                    size_t size,
                                    base::TimeTicks time) {
  ReceiveMidiData(jack_index, data, size, time);
}


void MidiManagerUsb::OnEnumerateDevicesDone(bool result,
                                            UsbMidiDevice::Devices* devices) {
  if (!result) {
    initialize_callback_.Run(MIDI_INITIALIZATION_ERROR);
    return;
  }
  input_stream_.reset(new UsbMidiInputStream(this));
  devices->swap(devices_);
  for (size_t i = 0; i < devices_.size(); ++i) {
    if (!AddPorts(devices_[i], i)) {
      initialize_callback_.Run(MIDI_INITIALIZATION_ERROR);
      return;
    }
  }
  initialize_callback_.Run(MIDI_OK);
}

bool MidiManagerUsb::AddPorts(UsbMidiDevice* device, int device_id) {
  UsbMidiDescriptorParser parser;
  std::vector<uint8> descriptor = device->GetDescriptor();
  const uint8* data = descriptor.size() > 0 ? &descriptor[0] : NULL;
  std::vector<UsbMidiJack> jacks;
  bool parse_result = parser.Parse(device,
                                   data,
                                   descriptor.size(),
                                   &jacks);
  if (!parse_result)
    return false;

  for (size_t j = 0; j < jacks.size(); ++j) {
    if (jacks[j].direction() == UsbMidiJack::DIRECTION_OUT) {
      output_streams_.push_back(new UsbMidiOutputStream(jacks[j]));
      // TODO(yhirano): Set appropriate properties.
      // TODO(yhiran): Port ID should contain product ID / vendor ID.
      // Port ID must be unique in a MIDI manager. This (and the below) ID
      // setting is sufficiently unique although there is no user-friendly
      // meaning.
      MidiPortInfo port;
      port.state = MIDI_PORT_OPENED;
      port.id = base::StringPrintf("port-%d-%ld",
                                   device_id,
                                   static_cast<long>(j));
      AddOutputPort(port);
    } else {
      DCHECK_EQ(jacks[j].direction(), UsbMidiJack::DIRECTION_IN);
      input_stream_->Add(jacks[j]);
      // TODO(yhirano): Set appropriate properties.
      MidiPortInfo port;
      port.state = MIDI_PORT_OPENED;
      port.id = base::StringPrintf("port-%d-%ld",
                                   device_id,
                                   static_cast<long>(j));
      AddInputPort(port);
    }
  }
  return true;
}

}  // namespace media
