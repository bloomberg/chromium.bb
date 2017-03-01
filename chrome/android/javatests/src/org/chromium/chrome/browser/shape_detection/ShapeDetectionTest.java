// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shape_detection;

import android.os.StrictMode;
import android.support.test.filters.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 *  Testing of the Shape Detection API. This API has three parts: QR/Barcodes,
 *  Text and Faces. Only the first two are tested here since Face detection
 *  is based on android.media.FaceDetector and doesn't need special treatment,
 *  hence is tested via content_browsertests.
 */
public class ShapeDetectionTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String BARCODE_TEST_EXPECTED_TAB_TITLE = "https://chromium.org";
    private static final String TEXT_TEST_EXPECTED_TAB_TITLE =
            "The quick brown fox jumped over the lazy dog. Helvetica Neue 36.";
    private StrictMode.ThreadPolicy mOldPolicy;

    public ShapeDetectionTest() {
        super(ChromeActivity.class);
    }

    /**
     * Verifies that QR codes are detected correctly.
     */
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @Feature({"ShapeDetection"})
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testBarcodeDetection() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        try {
            Tab tab = getActivity().getActivityTab();
            TabTitleObserver titleObserver =
                    new TabTitleObserver(tab, BARCODE_TEST_EXPECTED_TAB_TITLE);
            loadUrl(testServer.getURL("/chrome/test/data/android/barcode_detection.html"));
            titleObserver.waitForTitleUpdate(10);

            assertEquals(BARCODE_TEST_EXPECTED_TAB_TITLE, tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Verifies that text is detected correctly.
     */
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @Feature({"ShapeDetection"})
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTextDetection() throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        try {
            Tab tab = getActivity().getActivityTab();
            TabTitleObserver titleObserver =
                    new TabTitleObserver(tab, TEXT_TEST_EXPECTED_TAB_TITLE);
            loadUrl(testServer.getURL("/chrome/test/data/android/text_detection.html"));
            titleObserver.waitForTitleUpdate(10);

            assertEquals(TEXT_TEST_EXPECTED_TAB_TITLE, tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * We need to allow a looser policy due to the Google Play Services internals.
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOldPolicy = StrictMode.allowThreadDiskReads();
                StrictMode.allowThreadDiskWrites();
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                StrictMode.setThreadPolicy(mOldPolicy);
            }
        });
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
