// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * A test suite for zooming-related methods and settings.
 */
public class AwZoomTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    private String getZoomableHtml() {
        return "<html><head><meta name=\"viewport\" content=\"" +
                "width=device-width, minimum-scale=0.5, maximum-scale=2.0, initial-scale=0.5" +
                "\"/></head><body>Zoomable</body></html>";
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
                return mAwContents.isMultiTouchZoomSupported();
            }
        });
    }

    private int getVisibilityOnUiThread(final View view) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return view.getVisibility();
            }
        });
    }

    private View getZoomControlsOnUiThread() throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<View>() {
            @Override
            public View call() throws Exception {
                return mAwContents.getZoomControlsForTest();
            }
        });
    }

    private void invokeZoomPickerOnUiThread() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAwContents.invokeZoomPicker();
            }
        });
    }

    private boolean zoomInOnUiThreadAndWait() throws Throwable {
        final float previousScale = getPixelScaleOnUiThread(mAwContents);
        if (!runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mAwContents.zoomIn();
            }
        }))
            return false;
        // The zoom level is updated asynchronously.
        return waitForScaleChange(previousScale);
    }

    private boolean zoomOutOnUiThreadAndWait() throws Throwable {
        final float previousScale = getPixelScaleOnUiThread(mAwContents);
        if (!runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mAwContents.zoomOut();
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
                        return previousScale != getPixelScaleOnUiThread(mAwContents);
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to getPixelScaleOnUiThread: " + t.toString());
                        return false;
                    }
                }
            }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);
    }

    private boolean waitUntilCanZoomIn() throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return canZoomInOnUiThread(mAwContents);
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to query canZoomIn: " + t.toString());
                        return false;
                    }
                }
            }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);
    }

    private boolean waitUntilCanNotZoom() throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return !canZoomInOnUiThread(mAwContents) &&
                                !canZoomOutOnUiThread(mAwContents);
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to query canZoomIn/Out: " + t.toString());
                        return false;
                    }
                }
            }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);
    }

    private void runMagnificationTest() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        assertFalse("Should not be able to zoom in", canZoomInOnUiThread(mAwContents));
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);
        waitUntilCanZoomIn();
        assertTrue("Should be able to zoom in", canZoomInOnUiThread(mAwContents));
        assertFalse("Should not be able to zoom out", canZoomOutOnUiThread(mAwContents));

        while (canZoomInOnUiThread(mAwContents)) {
            assertTrue(zoomInOnUiThreadAndWait());
        }
        assertTrue("Should be able to zoom out", canZoomOutOnUiThread(mAwContents));

        while (canZoomOutOnUiThread(mAwContents)) {
            assertTrue(zoomOutOnUiThreadAndWait());
        }
        assertTrue("Should be able to zoom in", canZoomInOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMagnification() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setSupportZoom(true);
        runMagnificationTest();
    }

    // According to Android CTS test, zoomIn/Out must work
    // even if supportZoom is turned off.
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMagnificationWithZoomSupportOff() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setSupportZoom(false);
        runMagnificationTest();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomUsingMultiTouch() throws Throwable {
        AwSettings webSettings = getAwSettingsOnUiThread(mAwContents);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);

        assertTrue(webSettings.supportZoom());
        assertFalse(webSettings.getBuiltInZoomControls());
        assertFalse(isMultiTouchZoomSupportedOnUiThread());

        webSettings.setBuiltInZoomControls(true);
        assertTrue(isMultiTouchZoomSupportedOnUiThread());

        webSettings.setSupportZoom(false);
        assertFalse(isMultiTouchZoomSupportedOnUiThread());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomControls() throws Throwable {
        AwSettings webSettings = getAwSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        assertFalse("Should not be able to zoom in", canZoomInOnUiThread(mAwContents));
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(), "text/html", false);
        waitUntilCanZoomIn();
        // It must be possible to zoom in (or zoom out) for zoom controls to be shown
        assertTrue("Should be able to zoom in", canZoomInOnUiThread(mAwContents));

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
        assertEquals(View.VISIBLE, getVisibilityOnUiThread(zoomControls));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomControlsOnNonZoomableContent() throws Throwable {
        AwSettings webSettings = getAwSettingsOnUiThread(mAwContents);
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
        assertEquals(View.GONE, getVisibilityOnUiThread(zoomControls));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZoomControlsOnOrientationChange() throws Throwable {
        AwSettings webSettings = getAwSettingsOnUiThread(mAwContents);
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
