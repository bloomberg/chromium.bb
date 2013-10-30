// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import org.chromium.mojo_shell_apk.LibraryLoader;
import org.chromium.mojo_shell_apk.MojoMain;
import org.chromium.mojo_shell_apk.MojoView;

/**
 * Activity for managing the Mojo Shell.
 */
public class MojoShellActivity extends Activity {
    private static final String TAG = "MojoShellActivity";

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            LibraryLoader.ensureInitialized();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "libmojo_shell initialization failed.", e);
            finish();
            return;
        }

        MojoMain.init(this);
        setContentView(R.layout.mojo_shell_activity);

        String appUrl = getUrlFromIntent(getIntent());
        MojoMain.start(this, appUrl);
        Log.i(TAG, "Mojo started: " + appUrl);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
