// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_browsertests_apk;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.base.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
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
    @SuppressFBWarnings("DM_EXIT")
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        } catch (ProcessInitException e) {
            Log.i(TAG, "Cannot load content_browsertests:" +  e);
            System.exit(-1);
        }
        BrowserStartupController.get(getApplicationContext(), LibraryProcessType.PROCESS_BROWSER)
                .initChromiumBrowserProcessForTests();

        setContentView(R.layout.test_activity);
        mShellManager = (ShellManager) findViewById(R.id.shell_container);
        mWindowAndroid = new ActivityWindowAndroid(this);
        mShellManager.setWindow(mWindowAndroid, false);

        Window wind = this.getWindow();
        wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "Running tests");
                runTests();
                Log.i(TAG, "Tests finished.");
                finish();
            }
        });
    }

    private void runTests() {
        nativeRunTests(getFilesDir().getAbsolutePath(), getApplicationContext());
    }

    private native void nativeRunTests(String filesDir, Context appContext);
}
