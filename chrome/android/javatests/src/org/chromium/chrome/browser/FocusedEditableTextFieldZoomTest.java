// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.content.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.os.Environment;
import android.view.KeyEvent;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for zooming into & out of a selected & deselected editable text field.
 */
public class FocusedEditableTextFieldZoomTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final int TEST_TIMEOUT = 5000;
    private static final String TEXTFIELD_DOM_ID = "textfield";
    private static final float FLOAT_DELTA = 0.01f;
    private static final float INITIAL_SCALE = 0.5f;

    private EmbeddedTestServer mTestServer;

    public FocusedEditableTextFieldZoomTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    void waitForInitialZoom() throws InterruptedException {
        // The zoom level sometimes changes immediately after the page loads which makes grabbing
        // the initial value problematic. We solve this by explicitly specifying the initial zoom
        // level via the viewport tag and waiting for the zoom level to reach that value before we
        // proceed with the rest of the test.
        final ContentViewCore contentViewCore = getActivity().getActivityTab().getContentViewCore();
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return contentViewCore.getScale() - INITIAL_SCALE < FLOAT_DELTA;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    private void waitForZoomIn(final ContentViewCore contentViewCore, final float initialZoomLevel)
            throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return contentViewCore.getScale() > initialZoomLevel;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    @DisabledTest
    @Feature({"TabContents"})
    /*
     * @LargeTest
     * Broken by subpixel precision changes crbug.com/371119
     */
    public void testZoomInToSelected() throws Throwable {
        // This should focus the text field and initiate a zoom in.
        Tab tab = getActivity().getActivityTab();
        final ContentViewCore contentViewCore = tab.getContentViewCore();
        float initialZoomLevel = contentViewCore.getScale();

        DOMUtils.clickNode(this, contentViewCore, TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(contentViewCore, initialZoomLevel);
    }

    @DisabledTest
    @Feature({"TabContents"})
    /*
     * @LargeTest
     * Broken by subpixel precision changes crbug.com/371119
     */
    public void testZoomOutOfSelectedIfOnlyBackPressed() throws Throwable {
        final Tab tab = getActivity().getActivityTab();
        final ContentViewCore contentViewCore = tab.getContentViewCore();
        final float initialZoomLevel = contentViewCore.getScale();

        // This should focus the text field and initiate a zoom in.
        DOMUtils.clickNode(this, contentViewCore, TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(contentViewCore, initialZoomLevel);

        KeyUtils.singleKeyEventView(getInstrumentation(), tab.getView(), KeyEvent.KEYCODE_BACK);

        // We should zoom out to the previous zoom level.
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return (contentViewCore.getScale() - initialZoomLevel) < FLOAT_DELTA;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/focused_editable_zoom.html"));
        waitForInitialZoom();
    }
}
