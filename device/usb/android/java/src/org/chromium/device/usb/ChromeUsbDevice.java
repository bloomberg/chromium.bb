// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.annotation.TargetApi;
import android.hardware.usb.UsbConfiguration;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.hardware.usb.UsbDevice as necessary for C++
 * device::UsbDeviceAndroid.
 *
 * Lifetime is controlled by device::UsbDeviceAndroid.
 */
@JNINamespace("device")
final class ChromeUsbDevice {
    private static final String TAG = "Usb";

    final UsbDevice mDevice;

    private ChromeUsbDevice(UsbDevice device) {
        mDevice = device;
        Log.v(TAG, "ChromeUsbDevice created.");
    }

    @CalledByNative
    private static ChromeUsbDevice create(UsbDevice device) {
        return new ChromeUsbDevice(device);
    }

    @CalledByNative
    private int getVendorId() {
        return mDevice.getVendorId();
    }

    @CalledByNative
    private int getProductId() {
        return mDevice.getProductId();
    }

    @CalledByNative
    private String getManufacturerName() {
        return mDevice.getManufacturerName();
    }

    @CalledByNative
    private String getProductName() {
        return mDevice.getProductName();
    }

    @CalledByNative
    private String getSerialNumber() {
        return mDevice.getSerialNumber();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private UsbConfiguration[] getConfigurations() {
        int count = mDevice.getConfigurationCount();
        UsbConfiguration[] configurations = new UsbConfiguration[count];
        for (int i = 0; i < count; ++i) {
            configurations[i] = mDevice.getConfiguration(i);
        }
        return configurations;
    }

    @CalledByNative
    private UsbInterface[] getInterfaces() {
        int count = mDevice.getInterfaceCount();
        UsbInterface[] interfaces = new UsbInterface[count];
        for (int i = 0; i < count; ++i) {
            interfaces[i] = mDevice.getInterface(i);
        }
        return interfaces;
    }
}
