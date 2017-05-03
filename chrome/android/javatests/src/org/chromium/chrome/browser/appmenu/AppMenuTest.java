// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListPopupWindow;
import android.widget.ListView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Tests AppMenu popup
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class AppMenuTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>foo</html>");

    private AppMenu mAppMenu;
    private AppMenuHandler mAppMenuHandler;

    /**
     * AppMenuHandler that will be used to intercept item selections for testing.
     */
    @SuppressFBWarnings("URF_UNREAD_FIELD")
    public static class AppMenuHandlerForTest extends AppMenuHandler {
        int mLastSelectedItemId = -1;

        /**
         * AppMenuHandler for intercepting options item selections.
         */
        public AppMenuHandlerForTest(Activity activity, AppMenuPropertiesDelegate delegate,
                int menuResourceId) {
            super(activity, delegate, menuResourceId);
        }

        @Override
        void onOptionsItemSelected(MenuItem item) {
            mLastSelectedItemId = item.getItemId();
        }
    }

    @Before
    public void setUp() throws Exception {
        // We need list selection; ensure we are not in touch mode.
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);

        ChromeActivity.setAppMenuHandlerFactoryForTesting(
                new ChromeActivity.AppMenuHandlerFactory() {
                    @Override
                    public AppMenuHandler get(Activity activity, AppMenuPropertiesDelegate delegate,
                            int menuResourceId) {
                        mAppMenuHandler =
                                new AppMenuHandlerForTest(activity, delegate, menuResourceId);
                        return mAppMenuHandler;
                    }
                });

        mActivityTestRule.startMainActivityWithURL(TEST_URL);

        showAppMenuAndAssertMenuShown();
        mAppMenu = mActivityTestRule.getActivity().getAppMenuHandler().getAppMenu();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAppMenu.getPopup().getListView().setSelection(0);
            }
        });
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getCurrentFocusedRow();
            }
        }));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Verify opening a new tab from the menu.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testMenuNewTab() throws InterruptedException {
        final int tabCountBefore = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mActivityTestRule.getActivity());
        final int tabCountAfter = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        Assert.assertTrue("Expected: " + (tabCountBefore + 1) + " Got: " + tabCountAfter,
                tabCountBefore + 1 == tabCountAfter);
    }

    /**
     * Test bounds when accessing the menu through the keyboard.
     * Make sure that the menu stays open when trying to move past the first and last items.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuBoundaries() {
        moveToBoundary(false, true);
        Assert.assertEquals(getCount() - 1, getCurrentFocusedRow());
        moveToBoundary(true, true);
        Assert.assertEquals(0, getCurrentFocusedRow());
        moveToBoundary(false, true);
        Assert.assertEquals(getCount() - 1, getCurrentFocusedRow());
    }

    /**
     * Test that typing ENTER immediately opening the menu works.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnOpen() {
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the top item doesn't crash Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() {
        moveToBoundary(true, true);
        Assert.assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the bottom item doesn't crash Chrome.
     * Catches regressions for http://crbug.com/181067
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastBottomItem() {
        moveToBoundary(false, true);
        Assert.assertEquals(getCount() - 1, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item actually triggers the top item.
     * Catches regressions for https://crbug.com/191239 for shrunken menus.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemLandscape() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        Assert.assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item doesn't crash Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemPortrait() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        Assert.assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that changing orientation hides the menu.
     */
    /*
    @SmallTest
    @Feature({"Browser", "Main"})
    */
    @Test
    @DisabledTest(message = "crbug.com/458193")
    public void testChangingOrientationHidesMenu() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        CriteriaHelper.pollInstrumentationThread(new Criteria("AppMenu did not dismiss") {
            @Override
            public boolean isSatisfied() {
                return !mAppMenuHandler.isAppMenuShowing();
            }
        });
    }

    private void showAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAppMenuHandler.showAppMenu(null, false);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria("AppMenu did not show") {
            @Override
            public boolean isSatisfied() {
                return mAppMenuHandler.isAppMenuShowing();
            }
        });
    }

    private void hitEnterAndAssertAppMenuDismissed() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        pressKey(KeyEvent.KEYCODE_ENTER);
        CriteriaHelper.pollInstrumentationThread(new Criteria("AppMenu did not dismiss") {
            @Override
            public boolean isSatisfied() {
                return !mAppMenuHandler.isAppMenuShowing();
            }
        });
    }

    private void moveToBoundary(boolean towardsTop, boolean movePast) {
        // Move to the boundary.
        final int end = towardsTop ? 0 : getCount() - 1;
        int increment = towardsTop ? -1 : 1;
        for (int index = getCurrentFocusedRow(); index != end; index += increment) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            final int expectedPosition = index + increment;
            CriteriaHelper.pollInstrumentationThread(
                    Criteria.equals(expectedPosition, new Callable<Integer>() {
                        @Override
                        public Integer call() {
                            return getCurrentFocusedRow();
                        }
                    }));
        }

        // Try moving past it by one.
        if (movePast) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            CriteriaHelper.pollInstrumentationThread(Criteria.equals(end, new Callable<Integer>() {
                @Override
                public Integer call() {
                    return getCurrentFocusedRow();
                }
            }));
        }

        // The menu should stay open.
        Assert.assertTrue(mAppMenu.isShowing());
    }

    private void pressKey(final int keycode) {
        final View view = mAppMenu.getPopup().getListView();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keycode));
                view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keycode));
            }
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private int getCurrentFocusedRow() {
        ListPopupWindow popup = mAppMenu.getPopup();
        if (popup == null || popup.getListView() == null) return ListView.INVALID_POSITION;
        ListView listView = popup.getListView();
        return listView.getSelectedItemPosition();
    }

    private int getCount() {
        ListPopupWindow popup = mAppMenu.getPopup();
        if (popup == null || popup.getListView() == null) return 0;
        return popup.getListView().getCount();
    }
}
