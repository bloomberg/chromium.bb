// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.os.Build;

import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.test.util.DisableInTabbedMode;

/**
 * Base class for Instrumentation tests specific to DocumentActivity.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public abstract class DocumentActivityTestBase
        extends ChromeActivityTestCaseBase<DocumentActivity> {

    private static final String TAG = "DocumentActivityTestBase";

    public DocumentActivityTestBase() {
        super(org.chromium.chrome.browser.document.DocumentActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

}
