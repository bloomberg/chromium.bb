// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java implementations of SystemTimeChangeNotifierAndroid functionality.
 * Forwards TIME_SET intent to native SystemTimeChangeNotifierAndroid.
 */
@JNINamespace("chromecast")
public final class SystemTimeChangeNotifierAndroid {
    private final Context mContext;
    private BroadcastReceiver mTimeChangeObserver;

    @CalledByNative
    private static SystemTimeChangeNotifierAndroid create(Context context) {
        return new SystemTimeChangeNotifierAndroid(context);
    }

    private SystemTimeChangeNotifierAndroid(Context context) {
        mContext = context;
    }

    @CalledByNative
    private void initializeFromNative(final long nativeSystemTimeChangeNotifier) {
        // Listen to TIME_SET intent.
        mTimeChangeObserver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                nativeOnTimeChanged(nativeSystemTimeChangeNotifier);
            }
        };
        IntentFilter filter = new IntentFilter(Intent.ACTION_TIME_CHANGED);
        mContext.registerReceiver(mTimeChangeObserver, filter);
    }

    @CalledByNative private void finalizeFromNative() {
        mContext.unregisterReceiver(mTimeChangeObserver);
        mTimeChangeObserver = null;
    }

    private native void nativeOnTimeChanged(long nativeSystemTimeChangeNotifierAndroid);
}
