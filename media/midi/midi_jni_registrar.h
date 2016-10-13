// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_JNI_REGISTRAR_H_
#define MEDIA_MIDI_MIDI_JNI_REGISTRAR_H_

#include <jni.h>

#include "media/midi/midi_export.h"

namespace midi {

// Register all JNI bindings necessary for media/midi.
MIDI_EXPORT bool RegisterJni(JNIEnv* env);

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_JNI_REGISTRAR_H_
