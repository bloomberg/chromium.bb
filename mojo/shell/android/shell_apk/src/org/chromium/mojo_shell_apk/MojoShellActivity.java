// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.os.Bundle;
import android.app.Activity;
import android.util.Log;

import org.chromium.mojo_shell_apk.LibraryLoader;

/**
 * Activity for managing the Mojo Shell.
 */
public class MojoShellActivity extends Activity {
    private static final String TAG = "MojoShellActivity";

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.mojo_shell_activity);

        try {
            LibraryLoader.ensureInitialized();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "libmojo_shell initialization failed.", e);
            finish();
            return;
        }
        Log.i(TAG, "libmojo_shell initialization success.");
    }
}
