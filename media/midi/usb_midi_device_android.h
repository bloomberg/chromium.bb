// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_export.h"

namespace media {
namespace midi {

class USB_MIDI_EXPORT UsbMidiDeviceAndroid : public UsbMidiDevice {
 public:
  typedef base::android::ScopedJavaLocalRef<jobject> ObjectRef;

  static scoped_ptr<Factory> CreateFactory();

  UsbMidiDeviceAndroid(ObjectRef raw_device, UsbMidiDeviceDelegate* delegate);
  ~UsbMidiDeviceAndroid() override;

  // UsbMidiDevice implementation.
  std::vector<uint8> GetDescriptors() override;
  std::string GetManufacturer() override;
  std::string GetProductName() override;
  std::string GetDeviceVersion() override;
  void Send(int endpoint_number, const std::vector<uint8>& data) override;

  // Called by the Java world.
  void OnData(JNIEnv* env,
              jobject caller,
              jint endpoint_number,
              jbyteArray data);

  static bool RegisterUsbMidiDevice(JNIEnv* env);

 private:
  void GetDescriptorsInternal();
  void InitDeviceInfo();
  std::vector<uint8> GetStringDescriptor(int index);
  std::string GetString(int index, const std::string& backup);

  // The actual device object.
  base::android::ScopedJavaGlobalRef<jobject> raw_device_;
  UsbMidiDeviceDelegate* delegate_;

  std::vector<uint8> descriptors_;
  std::string manufacturer_;
  std::string product_;
  std::string device_version_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UsbMidiDeviceAndroid);
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
