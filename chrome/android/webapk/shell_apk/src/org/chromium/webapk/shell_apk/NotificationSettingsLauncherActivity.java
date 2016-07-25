// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Forwards NOTIFICATION_PREFERENCES intent to host browser.
 */
public class NotificationSettingsLauncherActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstance) {
        super.onCreate(savedInstance);

        String hostPackage = WebApkUtils.getHostBrowserPackageName(this);
        Intent intent = getIntent();
        intent.setPackage(hostPackage);
        intent.setComponent(null);
        startActivity(intent);
        finish();
    }
}
