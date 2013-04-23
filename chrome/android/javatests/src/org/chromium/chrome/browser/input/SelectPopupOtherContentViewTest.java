// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.ui.WindowAndroid;

import java.util.concurrent.TimeUnit;

public class SelectPopupOtherContentViewTest extends ChromiumTestShellTestBase {
    private static final int WAIT_TIMEOUT_SECONDS = 2;
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

    private static class PopupShowingCriteria implements Criteria {
        @Override
        public boolean isSatisfied() {
            return SelectPopupDialog.getCurrent() != null;
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
        launchChromiumTestShellWithUrl(SELECT_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(this, view, viewClient, "select");
        assertTrue("The select popup did not show up on click.",
                CriteriaHelper.pollForCriteria(new PopupShowingCriteria()));

        // Now create and destroy a different ContentView.
        UiUtils.runOnUiThread(getActivity(), new Runnable() {
            @Override
            public void run() {
                int nativeWebContents = ContentViewUtil.createNativeWebContents(false);
                WindowAndroid windowAndroid = new WindowAndroid(getActivity());
                ContentView contentView = ContentView.newInstance(
                        getActivity(), nativeWebContents,
                        windowAndroid, ContentView.PERSONALITY_CHROME);
                contentView.destroy();
            }
        });

        // Process some more events to give a chance to the dialog to hide if it were to.
        getInstrumentation().waitForIdleSync();

        // The popup should still be shown.
        assertNotNull("The select popup got hidden by destroying of unrelated ContentViewCore.",
                SelectPopupDialog.getCurrent());
    }
}
