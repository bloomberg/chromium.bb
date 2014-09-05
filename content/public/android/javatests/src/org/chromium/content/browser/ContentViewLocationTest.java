// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.MockLocationProvider;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Test suite for ensureing that Geolocation interacts as expected
 * with ContentView APIs - e.g. that it's started and stopped as the
 * ContentView is hidden or shown.
 */
public class ContentViewLocationTest extends ContentShellTestBase {

    private TestCallbackHelperContainer mTestCallbackHelperContainer;
    private TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper mJavascriptHelper;
    private MockLocationProvider mMockLocationProvider;

    private void hideContentViewOnUiThread() {
        getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    getContentViewCore().onHide();
                }
        });
    }

    private void showContentViewOnUiThread() {
        getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    getContentViewCore().onShow();
                }
        });
    }

    private void pollForPositionCallback() throws Throwable {
        mJavascriptHelper.evaluateJavaScript(getContentViewCore(),
                "positionCount = 0");
        mJavascriptHelper.waitUntilHasValue();
        assertEquals(0, Integer.parseInt(mJavascriptHelper.getJsonResultAndClear()));

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    mJavascriptHelper.evaluateJavaScript(getContentViewCore(), "positionCount");
                    try {
                        mJavascriptHelper.waitUntilHasValue();
                    } catch (Exception e) {
                        fail();
                    }
                    return Integer.parseInt(mJavascriptHelper.getJsonResultAndClear()) > 0;
                }
        }));
    }

    private void startGeolocationWatchPosition() throws Throwable {
        mJavascriptHelper.evaluateJavaScript(getContentViewCore(),
                "initiate_watchPosition();");
        mJavascriptHelper.waitUntilHasValue();
    }

    private void ensureGeolocationRunning(final boolean running) throws Exception {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMockLocationProvider.isRunning() == running;
            }
        }));
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mMockLocationProvider = new MockLocationProvider();
        LocationProviderFactory.setLocationProviderImpl(mMockLocationProvider);

        try {
            startActivityWithTestUrl("content/geolocation.html");
        } catch (Throwable t) {
            fail();
        }

        mTestCallbackHelperContainer = new TestCallbackHelperContainer(getContentViewCore());
        mJavascriptHelper = new OnEvaluateJavaScriptResultHelper();

        ensureGeolocationRunning(false);
    }

    @Override
    protected void tearDown() throws Exception {
         mMockLocationProvider.stopUpdates();
         super.tearDown();
    }

    @MediumTest
    @Feature({"Location"})
    public void testWatchHideShowStop() throws Throwable {

        startGeolocationWatchPosition();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        // Now hide the ContentView and ensure that geolocation stops.
        hideContentViewOnUiThread();
        ensureGeolocationRunning(false);

        mJavascriptHelper.evaluateJavaScript(getContentViewCore(),
                "positionCount = 0");
        mJavascriptHelper.waitUntilHasValue();

        // Show the ContentView again and ensure that geolocation starts again.
        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        // Navigate away and ensure that geolocation stops.
        loadUrl(getContentViewCore(), mTestCallbackHelperContainer,
              new LoadUrlParams("about:blank"));
        ensureGeolocationRunning(false);
    }

    @MediumTest
    @Feature({"Location"})
    public void testHideWatchResume() throws Throwable {
        hideContentViewOnUiThread();
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);
    }

    @MediumTest
    @Feature({"Location"})
    public void testWatchHideNewWatchShow() throws Throwable {
        startGeolocationWatchPosition();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        hideContentViewOnUiThread();

        // Make sure that when starting a new watch while paused we still don't
        // start up geolocation until we show the content view again.
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);
    }

    @MediumTest
    @Feature({"Location"})
    public void testHideWatchStopShow() throws Throwable {
        hideContentViewOnUiThread();
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        loadUrl(getContentViewCore(), mTestCallbackHelperContainer,
                new LoadUrlParams("about:blank"));
        showContentViewOnUiThread();
        ensureGeolocationRunning(false);
    }
}
