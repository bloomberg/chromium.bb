// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
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
    long mUsbServiceAndroid;
    BroadcastReceiver mUsbDeviceReceiver;

    private ChromeUsbService(Context context, long usbServiceAndroid) {
        mContext = context;
        mUsbServiceAndroid = usbServiceAndroid;
        registerForUsbDeviceIntentBroadcast();
        Log.v(TAG, "ChromeUsbService created.");
    }

    @CalledByNative
    private static ChromeUsbService create(Context context, long usbServiceAndroid) {
        return new ChromeUsbService(context, usbServiceAndroid);
    }

    @CalledByNative
    private Object[] getDevices() {
        UsbManager manager = (UsbManager) mContext.getSystemService(Context.USB_SERVICE);
        HashMap<String, UsbDevice> deviceList = manager.getDeviceList();
        return deviceList.values().toArray();
    }

    @CalledByNative
    private void close() {
        unregisterForUsbDeviceIntentBroadcast();
    }

    private native void nativeDeviceAttached(long nativeUsbServiceAndroid, UsbDevice device);

    private native void nativeDeviceDetached(long nativeUsbServiceAndroid, int deviceId);

    private void registerForUsbDeviceIntentBroadcast() {
        mUsbDeviceReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(intent.getAction())) {
                    nativeDeviceAttached(mUsbServiceAndroid, device);
                } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
                    nativeDeviceDetached(mUsbServiceAndroid, device.getDeviceId());
                }
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        mContext.registerReceiver(mUsbDeviceReceiver, filter);
    }

    private void unregisterForUsbDeviceIntentBroadcast() {
        mContext.unregisterReceiver(mUsbDeviceReceiver);
        mUsbDeviceReceiver = null;
    }
}
