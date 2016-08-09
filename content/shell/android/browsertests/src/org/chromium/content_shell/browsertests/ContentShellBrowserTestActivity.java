// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell.browsertests;

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

import java.io.File;

/** An Activity base class for running browser tests against ContentShell. */
public abstract class ContentShellBrowserTestActivity extends NativeBrowserTestActivity {

    private static final String TAG = "cr.native_test";

    private ShellManager mShellManager;
    private WindowAndroid mWindowAndroid;

    /** Deletes a file or directory along with any of its children.
     *
     *  Note that, like File.delete(), this returns false if the file or directory couldn't be
     *  fully deleted. This means that, in the directory case, some files may be deleted even if
     *  the entire directory couldn't be.
     *
     *  @param file The file or directory to delete.
     *  @return Whether or not the file or directory was deleted.
     */
    private static boolean deleteRecursive(File file) {
        if (file == null) return true;

        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                if (!deleteRecursive(child)) {
                    return false;
                }
            }
        }
        return file.delete();
    }

    /** Initializes the browser process.
     *
     *  This generally includes loading native libraries and switching to the native command line,
     *  among other things.
     *
     *  @param privateDataDirectory The private data directory to clear before starting the
     *      browser process. Can be null.
     *  @throws ProcessInitException if the native libraries cannot be loaded.
     */
    @Override
    @SuppressFBWarnings("DM_EXIT")
    protected void initializeBrowserProcess() {
        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER)
                    .ensureInitialized(getApplicationContext());
        } catch (ProcessInitException e) {
            Log.e(TAG, "Cannot load content_browsertests.", e);
            System.exit(-1);
        }
        BrowserStartupController.get(getApplicationContext(), LibraryProcessType.PROCESS_BROWSER)
                .initChromiumBrowserProcessForTests();

        setContentView(getTestActivityViewId());
        mShellManager = (ShellManager) findViewById(getShellManagerViewId());
        mWindowAndroid = new ActivityWindowAndroid(this);
        mShellManager.setWindow(mWindowAndroid, false);

        Window wind = this.getWindow();
        wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    }

    protected abstract int getTestActivityViewId();

    protected abstract int getShellManagerViewId();
}
