// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.MediumTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeoutException;

/**
 * Class which provides test coverage for Popup Zoomer.
 */
@RetryOnFailure
public class ContentViewPopupZoomerTest extends ContentShellTestBase {
    private static PopupZoomer findPopupZoomer(ViewGroup view) {
        assert view != null;
        for (int i = 0; i < view.getChildCount(); i++) {
            View child = view.getChildAt(i);
            if (child instanceof PopupZoomer) return (PopupZoomer) child;
        }
        return null;
    }

    private static class PopupShowingCriteria extends Criteria {
        private final ViewGroup mView;
        private final boolean mShouldBeShown;
        public PopupShowingCriteria(ViewGroup view, boolean shouldBeShown) {
            super(shouldBeShown ? "Popup did not get shown." : "Popup shown incorrectly.");
            mView = view;
            mShouldBeShown = shouldBeShown;
        }
        @Override
        public boolean isSatisfied() {
            PopupZoomer popup = findPopupZoomer(mView);
            boolean isVisibilitySet = popup == null ? false : popup.getVisibility() == View.VISIBLE;
            return isVisibilitySet ? mShouldBeShown : !mShouldBeShown;
        }
    }

    private static class PopupHasNonZeroDimensionsCriteria extends Criteria {
        private final ViewGroup mView;
        public PopupHasNonZeroDimensionsCriteria(ViewGroup view) {
            super("The zoomer popup has zero dimensions.");
            mView = view;
        }
        @Override
        public boolean isSatisfied() {
            PopupZoomer popup = findPopupZoomer(mView);
            if (popup == null) return false;
            return popup.getWidth() != 0 && popup.getHeight() != 0;
        }
    }

    private String generateTestUrl(int totalUrls, int targetIdAt, String targetId) {
        StringBuilder testUrl = new StringBuilder();
        testUrl.append("<html><body>");
        for (int i = 0; i < totalUrls; i++) {
            boolean isTargeted = i == targetIdAt;
            testUrl.append("<a href=\"data:text/html;utf-8,<html><head><script>"
                    + "function doesItWork() { return 'yes'; }</script></head></html>\""
                    + (isTargeted ? (" id=\"" + targetId + "\"") : "") + ">"
                    + "<small><sup>"
                    + (isTargeted ? "<b>" : "") + i + (isTargeted ? "</b>" : "")
                    + "</sup></small></a>");
        }
        testUrl.append("</small></div></body></html>");
        return UrlUtils.encodeHtmlDataUri(testUrl.toString());
    }

    public ContentViewPopupZoomerTest() {
    }

    /**
     * Tests that shows a zoomer popup and makes sure it has valid dimensions.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testPopupZoomerShowsUp() throws InterruptedException, TimeoutException {
        launchContentShellWithUrl(generateTestUrl(100, 15, "clickme"));
        waitForActiveShellToBeDoneLoading();

        final ContentViewCore viewCore = getContentViewCore();
        final ViewGroup view = viewCore.getContainerView();

        // The popup should be hidden before the click.
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria(view, false));

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(viewCore, "clickme");
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria(view, true));

        // The shown popup should have valid dimensions eventually.
        CriteriaHelper.pollInstrumentationThread(new PopupHasNonZeroDimensionsCriteria(view));
    }

    /**
     * Tests Popup zoomer hides when device back key is pressed.
     */
    @MediumTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testBackKeyDismissesPopupZoomer() throws InterruptedException, TimeoutException {
        launchContentShellWithUrl(generateTestUrl(100, 15, "clickme"));
        waitForActiveShellToBeDoneLoading();

        final ContentViewCore viewCore = getContentViewCore();
        final ViewGroup view = viewCore.getContainerView();

        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria(view, false));
        DOMUtils.clickNode(viewCore, "clickme");
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria(view, true));
        sendKeys(KeyEvent.KEYCODE_BACK);
        // When device key is pressed, popup zoomer should hide if already showing.
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria(view, false));
    }
}
