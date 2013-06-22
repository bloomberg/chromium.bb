// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_PORT_INFO_H_
#define MEDIA_MIDI_MIDI_PORT_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT MIDIPortInfo {
  MIDIPortInfo();
  MIDIPortInfo(const std::string& in_id,
               const std::string& in_manufacturer,
               const std::string& in_name,
               const std::string& in_version);

  MIDIPortInfo(const MIDIPortInfo& info);
  ~MIDIPortInfo();

  std::string id;
  std::string manufacturer;
  std::string name;
  std::string version;
};

typedef std::vector<MIDIPortInfo> MIDIPortInfoList;

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_PORT_INFO_H_
