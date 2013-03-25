// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * A test suite for zooming-related methods and settings.
 */
public class ContentViewZoomTest extends AwTestBase {
    private static final long TEST_TIMEOUT_MS = 20000L;
    private static final int CHECK_INTERVAL_MS = 100;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mContentViewCore = testContainerView.getContentViewCore();
    }

    private String getZoomableHtml() {
        StringBuilder sb = new StringBuilder();
        sb.append("<html><head><meta name=\"viewport\" content=\""
                  + "width=device-width, minimum-scale=0.5, maximum-scale=2.0, initial-scale=0.5"
                  + "\"/></head><body>");

        for (int i = 0; i < 1000; i++) {
            sb.append("This is body. ");
        }

        sb.append("</body></html>");
        return sb.toString();
    }

    private String getNonZoomableHtml() {
        // This page can't be zoomed because its viewport fully occupies
        // view area and is explicitly made non user-scalable.
        return "<html><head>" +
                "<meta name=\"viewport\" " +
                "content=\"width=device-width,height=device-height," +
                "initial-scale=1,maximum-scale=1,user-scalable=no\">" +
                "</head><body>Non-zoomable</body></html>";
    }

    private boolean isMultiTouchZoomSupportedOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContentViewCore.isMultiTouchZoomSupported();
            }
        });
    }

    private boolean canZoomInOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContentViewCore.canZoomIn();
            }
        });
    }

    private boolean canZoomOutOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContentViewCore.canZoomOut();
            }
        });
    }

    private float getScaleOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Float>() {
            @Override
            public Float call() throws Exception {
                return mContentViewCore.getScale();
            }
        });
    }

    private View getZoomControlsOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<View>() {
            @Override
            public View call() throws Exception {
                return mContentViewCore.getZoomControlsForTest();
            }
        });
    }

    private void invokeZoomPickerOnUiThread() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.invokeZoomPicker();
            }
        });
    }

    private boolean zoomInOnUiThreadAndWait() throws Throwable {
        final float previousScale = getScaleOnUiThread();
        if (!runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContentViewCore.zoomIn();
            }
           }))
            return false;
        // The zoom level is updated asynchronously.
        return waitForScaleChange(previousScale);
    }

    private boolean zoomOutOnUiThreadAndWait() throws Throwable {
        final float previousScale = getScaleOnUiThread();
        if (!runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContentViewCore.zoomOut();
            }
           }))
            return false;
        // The zoom level is updated asynchronously.
        return waitForScaleChange(previousScale);
    }

    private boolean waitForScaleChange(final float previousScale) throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return previousScale != getScaleOnUiThread();
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to getScaleOnUiThread: " + t.toString());
                        return false;
                    }
                }
            }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS);
    }

    private boolean waitUntilCanNotZoom() throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return !canZoomInOnUiThread() && !canZoomOutOnUiThread();
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to query canZoomIn/Out: " + t.toString());
                        return false;
                    }
                }
            }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS);
    }

    private boolean waitUntilCanNotZoomOut() throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return !canZoomOutOnUiThread();
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to query canZoomIn/Out: " + t.toString());
                        return false;
                    }
                }
            }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS);
    }

    private void runMagnificationTest(boolean supportZoom) throws Throwable {
        getContentSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        mAwContents.getSettings().setEnableFixedLayoutMode(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);
        // It takes some time for scaling to settle down.
        waitUntilCanNotZoomOut();
        getContentSettingsOnUiThread(mAwContents).setSupportZoom(supportZoom);
        assertTrue("Should be able to zoom in", canZoomInOnUiThread());
        assertFalse("Should not be able to zoom out", canZoomOutOnUiThread());

        while (canZoomInOnUiThread()) {
            assertTrue(zoomInOnUiThreadAndWait());
        }
        assertTrue("Should be able to zoom out", canZoomOutOnUiThread());

        while (canZoomOutOnUiThread()) {
            assertTrue(zoomOutOnUiThreadAndWait());
        }
        assertTrue("Should be able to zoom in", canZoomInOnUiThread());
    }

    /**
     * @SmallTest
     * @Feature({"AndroidWebView"})
     * BUG 153522
     */
    @FlakyTest
    public void testMagnification() throws Throwable {
        runMagnificationTest(true);
    }

    // According to Android CTS test, zoomIn/Out must work
    // even if supportZoom is turned off.
    /**
     * @SmallTest
     * @Feature({"AndroidWebView"})
     * BUG 153522
     */
    @FlakyTest
    public void testMagnificationWithZoomSupportOff() throws Throwable {
        runMagnificationTest(false);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomUsingMultiTouch() throws Throwable {
        ContentSettings webSettings = getContentSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        mAwContents.getSettings().setEnableFixedLayoutMode(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);

        assertTrue(webSettings.supportZoom());
        assertFalse(webSettings.getBuiltInZoomControls());
        assertFalse(isMultiTouchZoomSupportedOnUiThread());

        getContentSettingsOnUiThread(mAwContents).setBuiltInZoomControls(true);
        assertTrue(isMultiTouchZoomSupportedOnUiThread());

        getContentSettingsOnUiThread(mAwContents).setSupportZoom(false);
        assertFalse(isMultiTouchZoomSupportedOnUiThread());
    }

    /**
     * @SmallTest
     * @Feature({"AndroidWebView"})
     * BUG 153522
     */
    @FlakyTest
    public void testZoomControls() throws Throwable {
        ContentSettings webSettings = getContentSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        mAwContents.getSettings().setEnableFixedLayoutMode(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);

        assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(false);

        // With DisplayZoomControls set to false, attempts to display zoom
        // controls must be ignored.
        assertNull(getZoomControlsOnUiThread());
        invokeZoomPickerOnUiThread();
        assertNull(getZoomControlsOnUiThread());

        webSettings.setDisplayZoomControls(true);
        assertNull(getZoomControlsOnUiThread());
        invokeZoomPickerOnUiThread();
        View zoomControls = getZoomControlsOnUiThread();
        assertEquals(View.VISIBLE, zoomControls.getVisibility());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomControlsOnNonZoomableContent() throws Throwable {
        ContentSettings webSettings = getContentSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        mAwContents.getSettings().setEnableFixedLayoutMode(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getNonZoomableHtml(), "text/html", false);

        // ContentView must update itself according to the viewport setup.
        waitUntilCanNotZoom();

        assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(true);
        assertNull(getZoomControlsOnUiThread());
        invokeZoomPickerOnUiThread();
        View zoomControls = getZoomControlsOnUiThread();
        assertEquals(View.GONE, zoomControls.getVisibility());
    }

    /**
     * @SmallTest
     * @Feature({"AndroidWebView"})
     * BUG 153523
     */
    @DisabledTest
    public void testZoomControlsOnOrientationChange() throws Throwable {
        ContentSettings webSettings = getContentSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        mAwContents.getSettings().setEnableFixedLayoutMode(true);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);

        assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(true);
        invokeZoomPickerOnUiThread();

        // Now force an orientation change, and try to display the zoom picker
        // again. Make sure that we don't crash when the ZoomPicker registers
        // it's receiver.

        Activity activity = getActivity();
        int orientation = activity.getRequestedOrientation();
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        activity.setRequestedOrientation(orientation);
        invokeZoomPickerOnUiThread();

        // We may crash shortly (as the zoom picker has a short delay in it before
        // it tries to register it's BroadcastReceiver), so sleep to verify we don't.
        // The delay is encoded in ZoomButtonsController#ZOOM_CONTROLS_TIMEOUT,
        // if that changes we may need to update this test.
        Thread.sleep(ViewConfiguration.getZoomControlsTimeout());
    }
}
