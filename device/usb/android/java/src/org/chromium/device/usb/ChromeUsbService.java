// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.HashMap;

/**
 * Exposes android.hardware.usb.UsbManager as necessary for C++
 * device::UsbServiceAndroid.
 *
 * Lifetime is controlled by device::UsbServiceAndroid.
 */
@JNINamespace("device")
final class ChromeUsbService {
    private static final String TAG = "Usb";

    Context mContext;

    private ChromeUsbService(Context context) {
        mContext = context;
        Log.v(TAG, "ChromeUsbService created.");
    }

    @CalledByNative
    private static ChromeUsbService create(Context context) {
        return new ChromeUsbService(context);
    }

    @CalledByNative
    private Object[] getDevices() {
        UsbManager manager = (UsbManager) mContext.getSystemService(Context.USB_SERVICE);
        HashMap<String, UsbDevice> deviceList = manager.getDeviceList();
        return deviceList.values().toArray();
    }
}
