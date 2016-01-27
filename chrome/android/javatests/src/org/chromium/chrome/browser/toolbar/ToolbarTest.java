// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.widget.findinpage.FindToolbar;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for toolbar manager behavior.
 */
public class ToolbarTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public ToolbarTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void findInPageFromMenu() throws InterruptedException {
        MenuUtils.invokeCustomMenuActionSync(getInstrumentation(),
                getActivity(), R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                FindToolbar findToolbar = (FindToolbar) getActivity().findViewById(
                        R.id.find_toolbar);

                boolean isVisible = findToolbar != null && findToolbar.isShown();
                return (visible == isVisible) && !findToolbar.isAnimating();
            }
        });
    }

    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"Omnibox"})
    public void testFindInPageDismissedOnOmniboxFocus() throws InterruptedException {
        findInPageFromMenu();

        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        waitForFindInPageVisibility(false);
    }

}
