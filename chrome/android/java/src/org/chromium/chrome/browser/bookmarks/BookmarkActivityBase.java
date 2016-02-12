// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

/**
 * Activity base class that all the EnhancedBookmark activities inherit. Currently it's responsible
 * for ensuring native library initialization.
 */
abstract class BookmarkActivityBase extends AppCompatActivity {
    private static final String TAG = "BookmarkActivityBase";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Ensure that native library is loaded.
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }
}
