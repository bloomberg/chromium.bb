// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_SERVICE_ANDROID_H_
#define DEVICE_USB_USB_SERVICE_ANDROID_H_

#include <string>
#include <unordered_map>

#include "base/android/scoped_java_ref.h"
#include "device/usb/usb_service.h"

namespace device {

class UsbDeviceAndroid;

// USB service implementation for Android. This is a stub implementation that
// does not return any devices.
class UsbServiceAndroid : public UsbService {
 public:
  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  UsbServiceAndroid();
  ~UsbServiceAndroid() override;

  // UsbService:
  scoped_refptr<UsbDevice> GetDevice(const std::string& guid) override;
  void GetDevices(const GetDevicesCallback& callback) override;

  // Methods called by Java.
  void DeviceAttached(JNIEnv* env,
                      const base::android::JavaRef<jobject>& caller,
                      const base::android::JavaRef<jobject>& usb_device);
  void DeviceDetached(JNIEnv* env,
                      const base::android::JavaRef<jobject>& caller,
                      jint device_id);

 private:
  void AddDevice(scoped_refptr<UsbDeviceAndroid> device);

  std::unordered_map<jint, scoped_refptr<UsbDeviceAndroid>> devices_by_id_;
  std::unordered_map<std::string, scoped_refptr<UsbDeviceAndroid>>
      devices_by_guid_;

  // Java object org.chromium.device.usb.ChromeUsbService.
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace device

#endif  // DEVICE_USB_USB_SERVICE_ANDROID_H_
