// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for the app menu when Chrome Home is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class ChromeHomeAppMenuTest {
    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>foo</html>");
    private AppMenuHandler mAppMenuHandler;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mAppMenuHandler = mBottomSheetTestRule.getActivity().getAppMenuHandler();
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "see crbug.com/772000")
    public void testPageMenu() throws IllegalArgumentException, InterruptedException {
        mBottomSheetTestRule.loadUrl(TEST_URL);

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        AppMenuIconRowFooter iconRow = (AppMenuIconRowFooter) appMenu.getFooterView();

        assertFalse(iconRow.getForwardButtonForTests().isEnabled());
        assertTrue(iconRow.getBookmarkButtonForTests().isEnabled());
        // Only HTTP/S pages can be downloaded.
        assertFalse(iconRow.getDownloadButtonForTests().isEnabled());
        assertTrue(iconRow.getPageInfoButtonForTests().isEnabled());
        assertTrue(iconRow.getReloadButtonForTests().isEnabled());

        // Navigate backward, open the menu and assert forward button is enabled.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mAppMenuHandler.hideAppMenu();
            mBottomSheetTestRule.getActivity().getActivityTab().goBack();
        });

        showAppMenuAndAssertMenuShown();
        iconRow = (AppMenuIconRowFooter) appMenu.getFooterView();
        assertTrue(iconRow.getForwardButtonForTests().isEnabled());
    }

    @Test
    @SmallTest
    public void testTabSwitcherMenu() throws IllegalArgumentException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheetTestRule.getActivity().getLayoutManager().showOverview(false));

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();

        assertNull(appMenu.getFooterView());
    }

    private void showAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThread((Runnable) () -> mAppMenuHandler.showAppMenu(null, false));
        CriteriaHelper.pollUiThread(new Criteria("AppMenu did not show") {
            @Override
            public boolean isSatisfied() {
                return mAppMenuHandler.isAppMenuShowing();
            }
        });
    }
}
