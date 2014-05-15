// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

public class SelectPopupOtherContentViewTest extends ChromeShellTestBase {
    private static final String SELECT_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>" +
            "Which animal is the strongest:<br/>" +
            "<select id=\"select\">" +
            "<option>Black bear</option>" +
            "<option>Polar bear</option>" +
            "<option>Grizzly</option>" +
            "<option>Tiger</option>" +
            "<option>Lion</option>" +
            "<option>Gorilla</option>" +
            "<option>Chipmunk</option>" +
            "</select>" +
            "</body></html>");

    private class PopupShowingCriteria implements Criteria {
        @Override
        public boolean isSatisfied() {
            ContentViewCore contentViewCore = getActivity().getActiveContentViewCore();
            return contentViewCore.getSelectPopupForTest() != null;
        }
    }

    public SelectPopupOtherContentViewTest() {
    }

    /**
     * Tests that the showing select popup does not get closed because an unrelated ContentView
     * gets destroyed.
     *
     * @LargeTest
     * @Feature({"Browser"})
     * BUG 172967
    */
    @DisabledTest
    public void testPopupNotClosedByOtherContentView()
            throws InterruptedException, Exception, Throwable {
        // Load the test page.
        launchChromeShellWithUrl(SELECT_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final ContentViewCore viewCore = getActivity().getActiveContentViewCore();

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(this, viewCore, "select");
        assertTrue("The select popup did not show up on click.",
                CriteriaHelper.pollForCriteria(new PopupShowingCriteria()));

        // Now create and destroy a different ContentView.
        UiUtils.runOnUiThread(getActivity(), new Runnable() {
            @Override
            public void run() {
                long nativeWebContents = ContentViewUtil.createNativeWebContents(false);
                WindowAndroid windowAndroid = new ActivityWindowAndroid(getActivity());

                ContentViewCore contentViewCore = new ContentViewCore(getActivity());
                ContentView cv = ContentView.newInstance(getActivity(), contentViewCore);
                contentViewCore.initialize(cv, cv, nativeWebContents, windowAndroid);
                contentViewCore.destroy();
            }
        });

        // Process some more events to give a chance to the dialog to hide if it were to.
        getInstrumentation().waitForIdleSync();

        // The popup should still be shown.
        assertNotNull("The select popup got hidden by destroying of unrelated ContentViewCore.",
                viewCore.getSelectPopupForTest());
    }
}
