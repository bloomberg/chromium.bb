// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_browsertests_apk;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content_shell.ShellManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * Android activity for running content browser tests
 */
@JNINamespace("content")
public class ContentBrowserTestsActivity extends Activity {
    private static final String TAG = "ChromeBrowserTestsActivity";

    private ShellManager mShellManager;
    private WindowAndroid mWindowAndroid;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            LibraryLoader.ensureInitialized();
        } catch (ProcessInitException e) {
            Log.i(TAG, "Cannot load content_browsertests:" +  e);
            System.exit(-1);
        }
        BrowserStartupController.get(getApplicationContext()).initChromiumBrowserProcessForTests();

        LayoutInflater inflater =
                (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.test_activity, null);
        mShellManager = (ShellManager) view.findViewById(R.id.shell_container);
        mWindowAndroid = new ActivityWindowAndroid(this);
        mShellManager.setWindow(mWindowAndroid);

        Log.i(TAG, "Running tests");
        runTests();
        Log.i(TAG, "Tests finished.");
        finish();
    }

    private void runTests() {
        nativeRunTests(getFilesDir().getAbsolutePath(), getApplicationContext());
    }

    private native void nativeRunTests(String filesDir, Context appContext);
}
