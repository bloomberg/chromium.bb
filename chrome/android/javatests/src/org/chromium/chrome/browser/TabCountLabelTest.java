// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.MediumTest;
import android.widget.ImageButton;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;

/**
 * Test suite for the tab count widget on the phone toolbar.
 */

public class TabCountLabelTest extends ChromeTabbedActivityTestBase {

    /**
     * Check the tabCount string against an expected value.
     */
    private void tabCountLabelCheck(String stepName, int tabCountExpected) {
        ImageButton tabSwitcherBtn =
                (ImageButton) getActivity().findViewById(R.id.tab_switcher_button);
        TabSwitcherDrawable drawable = (TabSwitcherDrawable) tabSwitcherBtn.getDrawable();
        int tabCountFromDrawable = drawable.getTabCount();
        assertTrue(stepName + ", " + tabCountExpected + " tab[s] expected, label shows "
                + tabCountFromDrawable, tabCountExpected == tabCountFromDrawable);
    }

    /**
     * Verify displayed Tab Count matches the actual number of tabs.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testTabCountLabel() throws InterruptedException {
        final int tabCount = getActivity().getCurrentTabModel().getCount();
        tabCountLabelCheck("Initial state", tabCount);
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        // Make sure the TAB_CREATED notification went through
        getInstrumentation().waitForIdleSync();
        tabCountLabelCheck("After new tab", tabCount + 1);
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
        // Make sure the TAB_CLOSED notification went through
        getInstrumentation().waitForIdleSync();
        tabCountLabelCheck("After close tab", tabCount);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
