// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.BottomSheetTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for the app menu when Chrome Home is enabled.
 */
public class ChromeHomeAppMenuTest extends BottomSheetTestCaseBase {
    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>foo</html>");
    private AppMenuHandler mAppMenuHandler;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mAppMenuHandler = getActivity().getAppMenuHandler();
    }

    @SmallTest
    public void testPageMenu() throws IllegalArgumentException, InterruptedException {
        loadUrl(TEST_URL);

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = getActivity().getAppMenuHandler().getAppMenu();
        AppMenuIconRowFooter iconRow = (AppMenuIconRowFooter) appMenu.getPromptView();

        assertFalse(iconRow.getForwardButtonForTests().isEnabled());
        assertTrue(iconRow.getBookmarkButtonForTests().isEnabled());
        // Only HTTP/S pages can be downloaded.
        assertFalse(iconRow.getDownloadButtonForTests().isEnabled());
        assertTrue(iconRow.getPageInfoButtonForTests().isEnabled());
        assertTrue(iconRow.getReloadButtonForTests().isEnabled());

        // Navigate backward, open the menu and assert forward button is enabled.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAppMenuHandler.hideAppMenu();
                getActivity().getActivityTab().goBack();
            }
        });

        showAppMenuAndAssertMenuShown();
        iconRow = (AppMenuIconRowFooter) appMenu.getPromptView();
        assertTrue(iconRow.getForwardButtonForTests().isEnabled());
    }

    @SmallTest
    public void testTabSwitcherMenu() throws IllegalArgumentException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getLayoutManager().showOverview(false);
            }
        });

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = getActivity().getAppMenuHandler().getAppMenu();

        assertNull(appMenu.getPromptView());
    }

    private void showAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAppMenuHandler.showAppMenu(null, false);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("AppMenu did not show") {
            @Override
            public boolean isSatisfied() {
                return mAppMenuHandler.isAppMenuShowing();
            }
        });
    }
}
