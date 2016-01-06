// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbEndpoint;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.hardware.usb.UsbEndpoint as necessary for C++
 * device::UsbEndpointAndroid.
 *
 * Lifetime is controlled by device::UsbEndpointAndroid.
 */
@JNINamespace("device")
final class ChromeUsbEndpoint {
    private static final String TAG = "Usb";

    final UsbEndpoint mEndpoint;

    private ChromeUsbEndpoint(UsbEndpoint endpoint) {
        mEndpoint = endpoint;
        Log.v(TAG, "ChromeUsbEndpoint created.");
    }

    @CalledByNative
    private static ChromeUsbEndpoint create(UsbEndpoint endpoint) {
        return new ChromeUsbEndpoint(endpoint);
    }

    @CalledByNative
    private int getAddress() {
        return mEndpoint.getAddress();
    }

    @CalledByNative
    private int getDirection() {
        switch (mEndpoint.getDirection()) {
            case UsbConstants.USB_DIR_IN:
                return UsbEndpointDirection.USB_DIRECTION_INBOUND;
            case UsbConstants.USB_DIR_OUT:
                return UsbEndpointDirection.USB_DIRECTION_OUTBOUND;
            default:
                throw new AssertionError();
        }
    }

    @CalledByNative
    private int getMaxPacketSize() {
        return mEndpoint.getMaxPacketSize();
    }

    @CalledByNative
    private int getAttributes() {
        return mEndpoint.getAttributes();
    }

    @CalledByNative
    private int getType() {
        switch (mEndpoint.getType()) {
            case UsbConstants.USB_ENDPOINT_XFER_CONTROL:
                return UsbTransferType.USB_TRANSFER_CONTROL;
            case UsbConstants.USB_ENDPOINT_XFER_ISOC:
                return UsbTransferType.USB_TRANSFER_ISOCHRONOUS;
            case UsbConstants.USB_ENDPOINT_XFER_BULK:
                return UsbTransferType.USB_TRANSFER_BULK;
            case UsbConstants.USB_ENDPOINT_XFER_INT:
                return UsbTransferType.USB_TRANSFER_INTERRUPT;
            default:
                throw new AssertionError();
        }
    }

    @CalledByNative
    private int getInterval() {
        return mEndpoint.getInterval();
    }
}
