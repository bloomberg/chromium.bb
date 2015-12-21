// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AUDIO_NODE_H_
#define CHROMEOS_DBUS_AUDIO_NODE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Structure to hold AudioNode data received from cras.
struct CHROMEOS_EXPORT AudioNode {
  bool is_input;
  uint64_t id;
  std::string device_name;
  std::string type;
  std::string name;
  std::string mic_positions;
  bool active;
  // Time that the node was plugged in.
  uint64_t plugged_time;

  AudioNode();
  AudioNode(bool is_input,
            uint64_t id,
            std::string device_name,
            std::string type,
            std::string name,
            bool active,
            uint64_t plugged_time);
  ~AudioNode();
  std::string ToString() const;
};

typedef std::vector<AudioNode> AudioNodeList;

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AUDIO_NODE_H_
