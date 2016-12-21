// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.readermode.ReaderModePanel;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for making sure the distillability service is communicating correctly.
 */
@CommandLineFlags.Add({"enable-dom-distiller", "reader-mode-heuristics=alwaystrue"})
public class DistillabilityServiceTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    public DistillabilityServiceTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Make sure that Reader Mode appears after navigating from a native page.
     */
    @Feature({"Distillability-Service"})
    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest
    public void testServiceAliveAfterNativePage()
            throws InterruptedException, TimeoutException {

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                getInstrumentation().getContext());

        final ReaderModePanel panel = getActivity().getReaderModeManager().getPanelForTesting();

        TestWebContentsObserver observer =
                new TestWebContentsObserver(getActivity().getActivityTab().getWebContents());
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Navigate to a native page.
        int curCallCount = finishHelper.getCallCount();
        loadUrl("chrome://history");
        finishHelper.waitForCallback(curCallCount, 1);
        assertFalse(panel.isShowing());

        // Navigate to a normal page.
        curCallCount = finishHelper.getCallCount();
        loadUrl(testServer.getURL(TEST_PAGE));
        finishHelper.waitForCallback(curCallCount, 1);
        assertTrue(panel.isShowing());
    }
}
