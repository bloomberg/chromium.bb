// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_USB_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_USB_CHOOSER_DIALOG_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "device/usb/public/interfaces/chooser_service.mojom.h"
#include "device/usb/usb_service.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class UsbDevice;
}

// Represents a way to ask the user to select a USB device from a list of
// options.
class UsbChooserDialogAndroid : public device::UsbService::Observer {
 public:
  UsbChooserDialogAndroid(
      std::vector<device::mojom::UsbDeviceFilterPtr> filters,
      content::RenderFrameHost* render_frame_host,
      device::mojom::UsbChooserService::GetPermissionCallback callback);
  ~UsbChooserDialogAndroid() override;

  // device::UsbService::Observer:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  // Report the dialog's result.
  void OnItemSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& device_id);
  void OnDialogCancelled(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void LoadUsbHelpPage(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

 private:
  void GotUsbDeviceList(
      const std::vector<scoped_refptr<device::UsbDevice>>& devices);

  // Called when the user selects the USB device with |guid| from the chooser
  // dialog.
  void Select(const std::string& guid);
  // Called when the chooser dialog is closed.
  void Cancel();

  void AddDeviceToChooserDialog(scoped_refptr<device::UsbDevice> device) const;
  void RemoveDeviceFromChooserDialog(
      scoped_refptr<device::UsbDevice> device) const;

  void OpenUrl(const std::string& url);

  bool DisplayDevice(scoped_refptr<device::UsbDevice> device) const;

  content::RenderFrameHost* const render_frame_host_;
  device::mojom::UsbChooserService::GetPermissionCallback callback_;
  ScopedObserver<device::UsbService, device::UsbService::Observer>
      usb_service_observer_;
  std::vector<device::mojom::UsbDeviceFilterPtr> filters_;

  std::vector<scoped_refptr<device::UsbDevice>> devices_;

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;
  base::WeakPtrFactory<UsbChooserDialogAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_USB_CHOOSER_DIALOG_ANDROID_H_
