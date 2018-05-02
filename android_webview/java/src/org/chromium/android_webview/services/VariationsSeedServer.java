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
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

/**
 * VariationsSeedServer is a bound service that shares the Variations seed with all the WebViews
 * on the system. A WebView will bind and call getSeed, passing a file descriptor to which the
 * service should write the seed.
 */
public class VariationsSeedServer extends Service {
    private final IVariationsSeedServer.Stub mBinder = new IVariationsSeedServer.Stub() {
        @Override
        public void getSeed(ParcelFileDescriptor newSeedFile, long oldSeedDate) {
            // Write a minimally correct seed so that readSeedFile() can read it without error.
            // TODO(paulmiller): Write the actual seed, once seed downloading is implemented.
            SeedInfo mock = new SeedInfo();
            mock.signature = "";
            mock.country = "";
            mock.date = "Sun, 23 Jun 1912 00:00:00 GMT";
            mock.seedData = new byte[0];
            VariationsUtils.writeSeed(new AutoCloseOutputStream(newSeedFile), mock);
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
