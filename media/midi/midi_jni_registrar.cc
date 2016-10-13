// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "media/midi/midi_input_port_android.h"
#include "media/midi/midi_manager_android.h"
#include "media/midi/usb_midi_device_android.h"
#include "media/midi/usb_midi_device_factory_android.h"

namespace midi {

static base::android::RegistrationMethod kMediaRegisteredMethods[] = {
    {"UsbMidiDevice", UsbMidiDeviceAndroid::RegisterUsbMidiDevice},
    {"UsbMidiDeviceFactory",
     UsbMidiDeviceFactoryAndroid::RegisterUsbMidiDeviceFactory},
    {"MidiManagerAndroid", MidiManagerAndroid::Register},
    {"MidiInputPort", MidiInputPortAndroid::Register},
};

bool RegisterJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kMediaRegisteredMethods, arraysize(kMediaRegisteredMethods));
}

}  // namespace midi
