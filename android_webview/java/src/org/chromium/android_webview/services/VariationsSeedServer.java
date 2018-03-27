// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.ParcelFileDescriptor.AutoCloseOutputStream;

import org.chromium.android_webview.VariationsUtils;

/**
 * VariationsSeedServer is a bound service that shares the Variations seed with all the WebViews
 * on the system. A WebView will bind and call getSeed, passing a file descriptor to which the
 * service should write the seed.
 */
public class VariationsSeedServer extends Service {
    private final IVariationsSeedServer.Stub mBinder = new IVariationsSeedServer.Stub() {
        @Override
        public void getSeed(ParcelFileDescriptor newSeedFile, long oldSeedDate) {
            VariationsUtils.writeSeed(new AutoCloseOutputStream(newSeedFile), null);
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        ServiceInit.init(getApplicationContext());
    }
}
