// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// UsbMidiDevice represents a USB-MIDI device.
// This is an interface class and each platform-dependent implementation class
// will be a derived class.
class MEDIA_EXPORT UsbMidiDevice {
 public:
  virtual ~UsbMidiDevice() {}

  // Returns the descriptor of this device.
  virtual std::vector<uint8> GetDescriptor() = 0;

  // Sends |data| to the given USB endpoint of this device.
  virtual void Send(int endpoint_number, const std::vector<uint8>& data) = 0;
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_H_
