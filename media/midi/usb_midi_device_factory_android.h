// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_FACTORY_ANDROID_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_FACTORY_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_export.h"

namespace media {
namespace midi {

// This class enumerates UsbMidiDevices.
class USB_MIDI_EXPORT UsbMidiDeviceFactoryAndroid
      : public UsbMidiDevice::Factory {
 public:
  UsbMidiDeviceFactoryAndroid();
  ~UsbMidiDeviceFactoryAndroid() override;

  // UsbMidiDevice::Factory implementation.
  void EnumerateDevices(UsbMidiDeviceDelegate* delegate,
                        Callback callback) override;

  void OnUsbMidiDeviceRequestDone(JNIEnv* env,
                                  jobject caller,
                                  jobjectArray devices);
  void OnUsbMidiDeviceAttached(JNIEnv* env, jobject caller, jobject device);
  void OnUsbMidiDeviceDetached(JNIEnv* env, jobject caller, jint index);

  static bool RegisterUsbMidiDeviceFactory(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_factory_;
  // Not owned.
  UsbMidiDeviceDelegate* delegate_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(UsbMidiDeviceFactoryAndroid);
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_FACTORY_ANDROID_H_
