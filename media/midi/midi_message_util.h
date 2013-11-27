// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MESSAGE_UTIL_H_
#define MEDIA_MIDI_MIDI_MESSAGE_UTIL_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// Returns the length of a MIDI message in bytes. Never returns 4 or greater.
// Returns 0 if |status_byte| is:
// - not a valid status byte, namely data byte.
// - the MIDI System Exclusive message.
// - the End of System Exclusive message.
MEDIA_EXPORT size_t GetMIDIMessageLength(uint8 status_byte);

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MESSAGE_UTIL_H_
