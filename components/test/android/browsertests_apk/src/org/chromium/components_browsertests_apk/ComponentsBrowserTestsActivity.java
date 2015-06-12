// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components_browsertests_apk;

import android.os.Bundle;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content_shell.ShellManager;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * Android activity for running components browser tests
 */
public class ComponentsBrowserTestsActivity extends NativeBrowserTestActivity {
    private static final String TAG = Log.makeTag("native_test");

    private ShellManager mShellManager;
    private WindowAndroid mWindowAndroid;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name components_browsertests_devtools_remote");
    }

    @Override
    @SuppressFBWarnings("DM_EXIT")
    protected void initializeBrowserProcess() {
        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Cannot load components_browsertests.", e);
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
    }
}
