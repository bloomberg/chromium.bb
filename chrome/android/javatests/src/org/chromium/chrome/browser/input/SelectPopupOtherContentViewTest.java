// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import android.support.test.filters.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Test the select popup and how it interacts with another ContentViewCore.
 */
public class SelectPopupOtherContentViewTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String SELECT_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>"
            + "Which animal is the strongest:<br/>"
            + "<select id=\"select\">"
            + "<option>Black bear</option>"
            + "<option>Polar bear</option>"
            + "<option>Grizzly</option>"
            + "<option>Tiger</option>"
            + "<option>Lion</option>"
            + "<option>Gorilla</option>"
            + "<option>Chipmunk</option>"
            + "</select>"
            + "</body></html>");

    private class PopupShowingCriteria extends Criteria {
        public PopupShowingCriteria() {
            super("The select popup did not show up on click.");
        }

        @Override
        public boolean isSatisfied() {
            ContentViewCore contentViewCore = getActivity().getCurrentContentViewCore();
            return contentViewCore.getSelectPopupForTest() != null;
        }
    }

    public SelectPopupOtherContentViewTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Don't launch activity automatically.
    }

    /**
     * Tests that the showing select popup does not get closed because an unrelated ContentView
     * gets destroyed.
     *
     */
    @LargeTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testPopupNotClosedByOtherContentView()
            throws InterruptedException, Exception, Throwable {
        // Load the test page.
        startMainActivityWithURL(SELECT_URL);

        final ContentViewCore viewCore = getActivity().getCurrentContentViewCore();

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(viewCore, "select");
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria());

        // Now create and destroy a different ContentView.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WebContents webContents = WebContentsFactory.createWebContents(false, false);
                WindowAndroid windowAndroid = new ActivityWindowAndroid(getActivity());

                ContentViewCore contentViewCore = new ContentViewCore(getActivity(), "");
                ContentView cv = ContentView.createContentView(getActivity(), contentViewCore);
                contentViewCore.initialize(ViewAndroidDelegate.createBasicDelegate(cv), cv,
                        webContents, windowAndroid);
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
