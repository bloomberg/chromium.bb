// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.webapk.lib.runtime_library.WebApkServiceImpl;

/**
 * Implements services offered by the WebAPK to Chrome.
 */
public class WebApkServiceFactory extends Service {

    @Override
    public IBinder onBind(Intent intent) {
        return new WebApkServiceImpl(this, R.drawable.app_icon);
    }
}
