// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.content.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for zooming into & out of a selected & deselected editable text field.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class FocusedEditableTextFieldZoomTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int TEST_TIMEOUT = 5000;
    private static final String TEXTFIELD_DOM_ID = "textfield";
    private static final float FLOAT_DELTA = 0.01f;
    private static final float INITIAL_SCALE = 0.5f;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/focused_editable_zoom.html"));
        waitForInitialZoom();
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    void waitForInitialZoom() {
        // The zoom level sometimes changes immediately after the page loads which makes grabbing
        // the initial value problematic. We solve this by explicitly specifying the initial zoom
        // level via the viewport tag and waiting for the zoom level to reach that value before we
        // proceed with the rest of the test.
        final ContentViewCore contentViewCore =
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return contentViewCore.getPageScaleFactor() - INITIAL_SCALE < FLOAT_DELTA;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    private void waitForZoomIn(final ContentViewCore contentViewCore,
            final float initialZoomLevel) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return contentViewCore.getPageScaleFactor() > initialZoomLevel;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /*
     * @LargeTest
     */
    @Test
    @DisabledTest(message = "Broken by subpixel precision changes crbug.com/371119")
    @Feature({"TabContents"})
    public void testZoomInToSelected() throws Throwable {
        // This should focus the text field and initiate a zoom in.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final ContentViewCore contentViewCore = tab.getContentViewCore();
        float initialZoomLevel = contentViewCore.getPageScaleFactor();

        DOMUtils.clickNode(contentViewCore, TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(contentViewCore, initialZoomLevel);
    }

    /*
     * @LargeTest
     */
    @Test
    @DisabledTest(message = "Broken by subpixel precision changes crbug.com/371119")
    @Feature({"TabContents"})
    public void testZoomOutOfSelectedIfOnlyBackPressed() throws Throwable {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final ContentViewCore contentViewCore = tab.getContentViewCore();
        final float initialZoomLevel = contentViewCore.getPageScaleFactor();

        // This should focus the text field and initiate a zoom in.
        DOMUtils.clickNode(contentViewCore, TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(contentViewCore, initialZoomLevel);

        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), tab.getView(), KeyEvent.KEYCODE_BACK);

        // We should zoom out to the previous zoom level.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return (contentViewCore.getPageScaleFactor() - initialZoomLevel) < FLOAT_DELTA;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }
}
