// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.StrictMode;
import android.support.test.filters.LargeTest;

import org.chromium.base.test.util.Feature;

/**
 * Tests ensuring that starting up WebView does not cause any diskRead StrictMode violations.
 */
public class AwStrictModeTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mAwTestContainerView;

    private StrictMode.ThreadPolicy mOldThreadPolicy;
    private StrictMode.VmPolicy mOldVmPolicy;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        enableStrictModeOnUiThreadSync();
    }

    @Override
    protected void tearDown() throws Exception {
        disableStrictModeOnUiThreadSync();
        super.tearDown();
    }

    @Override
    protected boolean needsAwBrowserContextCreated() {
        return false;
    }


    @Override
    protected boolean needsBrowserProcessStarted() {
        // Don't start the browser process in AwTestBase - we want to start it ourselves with
        // strictmode policies turned on.
        return false;
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testStartup() throws Exception {
        startEverythingSync();
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testLoadEmptyData() throws Exception {
        startEverythingSync();
        loadDataSync(mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), "", "text/html", false);
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testSetJavaScriptAndLoadData() throws Exception {
        startEverythingSync();
        enableJavaScriptOnUiThread(mAwTestContainerView.getAwContents());
        loadDataSync(mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), "", "text/html", false);
    }

    private void enableStrictModeOnUiThreadSync() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mOldThreadPolicy = StrictMode.getThreadPolicy();
                mOldVmPolicy = StrictMode.getVmPolicy();
                StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                        .detectAll()
                        .penaltyLog()
                        .penaltyDeath()
                        .build());
                StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                        .detectAll()
                        .penaltyLog()
                        .penaltyDeath()
                        .build());
            }});
    }

    private void disableStrictModeOnUiThreadSync() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                StrictMode.setThreadPolicy(mOldThreadPolicy);
                StrictMode.setVmPolicy(mOldVmPolicy);
            }});
    }

    private void startEverythingSync() throws Exception {
        getActivity();
        createAwBrowserContext();
        startBrowserProcess();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwTestContainerView = createAwTestContainerView(mContentsClient);
            }
        });
    }
}
