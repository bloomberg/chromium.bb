// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.os.Build;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java implementation of CastSysInfoAndroid methods.
 */
@JNINamespace("chromecast")
public final class CastSysInfoAndroid {
    private static final String TAG = "CastSysInfoAndroid";

    @CalledByNative
    private static String getSerialNumber() {
        return Build.SERIAL;
    }
}
