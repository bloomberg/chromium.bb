// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.ParcelFileDescriptor;
import android.support.test.filters.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

import java.io.IOException;

/**
 * Instrumentation tests for Share intents.
 */
public class ShareIntentTest extends ChromeTabbedActivityTestBase {
    private static final String TAG = "ShareIntentTest";

    /**
     * Mock activity class that overrides the startActivity and checks if the file passed in the
     * intent can be opened.
     *
     * This class is a wrapper around the actual activity of the test, while it also inherits from
     * activity and redirects the calls to the methods to the actual activity.
     */
    private static class MockChromeActivity extends ChromeTabbedActivity {
        private Object mLock = new Object();
        private boolean mCheckCompleted = false;
        private ChromeActivity mActivity = null;

        public MockChromeActivity(ChromeActivity activity) {
            mActivity = activity;
            mCheckCompleted = false;
        }

        /**
         * Overrides startActivity and notifies check completed when the file from the uri of the
         * intent is opened.
         */
        @Override
        public void startActivity(Intent intent) {
            final Uri uri = intent.getClipData().getItemAt(0).getUri();
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... params) {
                    ChromeFileProvider provider = new ChromeFileProvider();
                    ParcelFileDescriptor file = null;
                    try {
                        file = provider.openFile(uri, "r");
                        if (file != null) file.close();
                    } catch (IOException e) {
                        assert false : "Error while opening the file";
                    }
                    synchronized (mLock) {
                        mCheckCompleted = true;
                        mLock.notify();
                    }
                    return null;
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        /**
         * Waits till the check for file opening is completed.
         */
        public void waitForFileCheck() throws InterruptedException {
            synchronized (mLock) {
                while (!mCheckCompleted) {
                    mLock.wait();
                }
            }
        }

        @Override
        public String getPackageName() {
            return mActivity.getPackageName();
        }

        @Override
        public Tab getActivityTab() {
            return mActivity.getActivityTab();
        }

        @Override
        public ChromeApplication getChromeApplication() {
            return mActivity.getChromeApplication();
        }

        @Override
        public PackageManager getPackageManager() {
            return mActivity.getPackageManager();
        }
    }

    @LargeTest
    @RetryOnFailure
    public void testShareIntent() {
        final MockChromeActivity mockActivity = new MockChromeActivity(getActivity());
        // Sets a test component as last shared and "shareDirectly" option is set so that the share
        // selector menu is not opened. The start activity is overriden, so the package and class
        // names do not matter.
        ShareHelper.setLastShareComponentName(new ComponentName("test.package", "test.activity"));
        // Skips the capture of screenshot and notifies with an empty file.
        mockActivity.setScreenshotCaptureSkippedForTesting(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mockActivity.onShareMenuItemSelected(
                        true /* shareDirectly */, false /* isIncognito */);
            }
        });

        try {
            mockActivity.waitForFileCheck();
        } catch (InterruptedException e) {
            assert false : "Test thread was interrupted while trying to wait.";
        }

        ShareHelper.setLastShareComponentName(new ComponentName("", ""));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
