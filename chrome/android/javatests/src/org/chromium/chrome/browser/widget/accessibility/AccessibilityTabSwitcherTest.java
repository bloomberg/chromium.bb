// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.accessibility;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.R;
import org.chromium.chrome.shell.TabManager;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Test suite for the accessibility tab switcher.
 *
 * For future tests, be wary of the button IDs being used and the R files being included. Some IDs
 * use the same name in multiple R files and are completely incompatible.
 */
public class AccessibilityTabSwitcherTest extends ChromeShellTestBase {
    private static final String PAGE_1_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%201<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";
    private static final String PAGE_2_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%202<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";

    private CharSequence getTabTitleOfListItem(final int index) throws Exception {
        final AccessibilityTabModelListView accessibilityView =
                (AccessibilityTabModelListView) getActivity().findViewById(R.id.list_view);
        assertTrue(accessibilityView.getCount() > index);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return accessibilityView.getChildCount() > index;
            }
        }));

        View childView = accessibilityView.getChildAt(0);
        TextView childTextView =
                (TextView) childView.findViewById(org.chromium.chrome.R.id.tab_title);
        return childTextView.getText();
    }

    private void toggleTabSwitcher(final boolean expectVisible) throws Exception {
        final TabManager tabManager = (TabManager) getActivity().findViewById(R.id.tab_manager);
        TouchCommon.singleClickView(getActivity().findViewById(R.id.tab_switcher));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return tabManager.isTabSwitcherVisible() == expectVisible;
            }
        }));
    }

    @MediumTest
    @Feature({"Accessibility"})
    public void testCanEnterAndLeaveSwitcher() throws Exception {
        launchChromeShellWithUrl(PAGE_1_HTML);
        waitForActiveShellToBeDoneLoading();

        final TabManager tabManager = (TabManager) getActivity().findViewById(R.id.tab_manager);
        assertFalse(tabManager.isTabSwitcherVisible());

        toggleTabSwitcher(true);
        assertEquals("Page 1", getTabTitleOfListItem(0));
        toggleTabSwitcher(false);
    }

    /**
     * Specifically tests that the TabObserver of the {@link AccessibilityTabModelListItem} is added
     * back to the Tab after the View is hidden.  This requires bringing the tab switcher back twice
     * because the TabObserver is removed/added when the tab switcher's View is detached from/
     * attached to the window.
     */
    @MediumTest
    @Feature({"Accessibility"})
    public void testObservesTitleChanges() throws Exception {
        launchChromeShellWithUrl(PAGE_1_HTML);
        waitForActiveShellToBeDoneLoading();

        final TabManager tabManager = (TabManager) getActivity().findViewById(R.id.tab_manager);
        assertFalse(tabManager.isTabSwitcherVisible());

        // Bring the tab switcher forward and send it away twice.
        toggleTabSwitcher(true);
        assertEquals("Page 1", getTabTitleOfListItem(0));
        toggleTabSwitcher(false);
        toggleTabSwitcher(true);
        toggleTabSwitcher(false);

        // Load another URL.
        final TabLoadObserver observer =
                new TabLoadObserver(getActivity().getActiveTab(), PAGE_2_HTML);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(observer));

        // Bring the tab switcher forward and check the title.
        toggleTabSwitcher(true);
        assertEquals("Page 2", getTabTitleOfListItem(0));
    }
}