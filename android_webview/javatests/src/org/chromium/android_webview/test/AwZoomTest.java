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

import java.util.Locale;
import java.util.concurrent.Callable;

/**
 * A test suite for zooming-related methods and settings.
 */
public class AwZoomTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private static float MAXIMUM_SCALE = 2.0f;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    private String getZoomableHtml(float scale) {
        final int divWidthPercent = (int)(100.0f / scale);
        return String.format(Locale.US, "<html><head><meta name=\"viewport\" content=\"" +
                "width=device-width, minimum-scale=%f, maximum-scale=%f, initial-scale=%f" +
                "\"/></head><body style='margin:0'>" +
                "<div style='width:%d%%;height:100px;border:1px solid black'>Zoomable</div>" +
                "</body></html>",
                scale, MAXIMUM_SCALE, scale, divWidthPercent);
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

    private void zoomInOnUiThreadAndWait() throws Throwable {
        final float previousScale = getPixelScaleOnUiThread(mAwContents);
        assertTrue(runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mAwContents.zoomIn();
            }
        }));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void zoomOutOnUiThreadAndWait() throws Throwable {
        final float previousScale = getPixelScaleOnUiThread(mAwContents);
        assertTrue(runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mAwContents.zoomOut();
            }
        }));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void zoomByOnUiThreadAndWait(final float delta) throws Throwable {
        final float previousScale = getPixelScaleOnUiThread(mAwContents);
        assertTrue(runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mAwContents.zoomBy(delta);
            }
        }));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void waitForScaleChange(final float previousScale) throws Throwable {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return previousScale != getPixelScaleOnUiThread(mAwContents);
            }
        });
    }

    private void waitForScaleToBecome(final float expectedScale) throws Throwable {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return expectedScale == getScaleOnUiThread(mAwContents);
            }
        });
    }

    private void waitUntilCanNotZoom() throws Throwable {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return !canZoomInOnUiThread(mAwContents) &&
                        !canZoomOutOnUiThread(mAwContents);
            }
        });
    }

    private void runMagnificationTest() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        assertFalse("Should not be able to zoom in", canZoomInOnUiThread(mAwContents));
        final float pageMinimumScale = 0.5f;
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale), "text/html", false);
        waitForScaleToBecome(pageMinimumScale);
        assertTrue("Should be able to zoom in", canZoomInOnUiThread(mAwContents));
        assertFalse("Should not be able to zoom out", canZoomOutOnUiThread(mAwContents));

        while (canZoomInOnUiThread(mAwContents)) {
            zoomInOnUiThreadAndWait();
        }
        assertTrue("Should be able to zoom out", canZoomOutOnUiThread(mAwContents));

        while (canZoomOutOnUiThread(mAwContents)) {
            zoomOutOnUiThreadAndWait();
        }
        assertTrue("Should be able to zoom in", canZoomInOnUiThread(mAwContents));

        zoomByOnUiThreadAndWait(4.0f);
        waitForScaleToBecome(MAXIMUM_SCALE);

        zoomByOnUiThreadAndWait(0.5f);
        waitForScaleToBecome(MAXIMUM_SCALE * 0.5f);

        zoomByOnUiThreadAndWait(0.01f);
        waitForScaleToBecome(pageMinimumScale);
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
                getZoomableHtml(0.5f), "text/html", false);

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
        final float pageMinimumScale = 0.5f;
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale), "text/html", false);
        waitForScaleToBecome(pageMinimumScale);
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
                getZoomableHtml(0.5f), "text/html", false);

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
