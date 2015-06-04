// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * Chrome browser connection service, used by the embedded Chrome activities.
 */
public class ChromeConnectionService extends Service {
    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return (IBinder) ChromeBrowserConnection.getInstance(getApplication());
    }

    @Override
    public boolean onUnbind(Intent intent) {
        super.onUnbind(intent);
        return false; // No support for onRebind().
    }
}
