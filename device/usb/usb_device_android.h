// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_ANDROID_H_
#define DEVICE_USB_USB_DEVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "device/usb/usb_device.h"

namespace device {

class UsbDeviceAndroid : public UsbDevice {
 public:
  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  static scoped_refptr<UsbDeviceAndroid> Create(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& usb_device);

  // UsbDevice:
  void Open(const OpenCallback& callback) override;
  const UsbConfigDescriptor* GetActiveConfiguration() override;

 private:
  UsbDeviceAndroid(JNIEnv* env,
                   uint16_t vendor_id,
                   uint16_t product_id,
                   const base::string16& manufacturer_string,
                   const base::string16& product_string,
                   const base::string16& serial_number,
                   const base::android::JavaRef<jobject>& wrapper);
  ~UsbDeviceAndroid() override;

  // Java object org.chromium.device.usb.ChromeUsbDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_ANDROID_H_
