// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.os.Bundle;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_shell.browsertests.ContentShellBrowserTestActivity;

import java.io.File;

/**
 * Android activity for running chrome browser tests.
 */
public class ChromeBrowserTestsActivity extends ContentShellBrowserTestActivity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name android_browsertests_devtools_remote");

        // TODO(danakj): This sets up some of the Chrome Java stuff.
        // AsyncInitializationActivity normally does this for the ChromeActivity.
        // We skip handlePostNativeStartup() for now, which runs child processes.
        // We should probably be a SynchronousInitializationActivity, which would
        // run both of these from onCreate(). But it's not clear yet how we
        // should be initializing the Chrome UI code (esp ChromeWindow and
        // CompositorViewHolder) vs using ContentShellBrowserTestActivity and
        // its ShellManager.
        BrowserParts parts = new EmptyBrowserParts() {};
        ChromeBrowserInitializer.getInstance(getApplicationContext()).handlePreNativeStartup(parts);
    }

    @Override
    protected void initializeBrowserProcess() {
        super.initializeBrowserProcess();
    }

    @Override
    protected File getPrivateDataDirectory() {
        // TODO(agrieve): We should not be touching the side-loaded test data directory.
        //     https://crbug.com/617734
        return new File(UrlUtils.getIsolatedTestRoot(),
                ChromeBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    protected int getTestActivityViewId() {
        return R.layout.test_activity;
    }

    @Override
    protected int getShellManagerViewId() {
        return R.id.shell_container;
    }
}
