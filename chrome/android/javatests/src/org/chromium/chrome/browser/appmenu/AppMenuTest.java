// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.support.test.filters.SmallTest;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListPopupWindow;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Tests AppMenu popup
 */
@RetryOnFailure
public class AppMenuTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>poit.</html>");

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

    public AppMenuTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(TEST_URL);
    }

    @Override
    protected void setUp() throws Exception {
        // We need list selection; ensure we are not in touch mode.
        getInstrumentation().setInTouchMode(false);

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

        super.setUp();

        showAppMenuAndAssertMenuShown();
        mAppMenu = getActivity().getAppMenuHandler().getAppMenu();
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
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Verify opening a new tab from the menu.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testMenuNewTab() throws InterruptedException {
        final int tabCountBefore = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), (ChromeTabbedActivity) getActivity());
        final int tabCountAfter = getActivity().getCurrentTabModel().getCount();
        assertTrue("Expected: " + (tabCountBefore + 1) + " Got: " + tabCountAfter,
                tabCountBefore + 1 == tabCountAfter);
    }

    /**
     * Test bounds when accessing the menu through the keyboard.
     * Make sure that the menu stays open when trying to move past the first and last items.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuBoundaries() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
    }

    /**
     * Test that typing ENTER immediately opening the menu works.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnOpen() {
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the top item doesn't crash Chrome.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() {
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the bottom item doesn't crash Chrome.
     * Catches regressions for http://crbug.com/181067
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastBottomItem() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item actually triggers the top item.
     * Catches regressions for https://crbug.com/191239 for shrunken menus.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemLandscape() {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item doesn't crash Chrome.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemPortrait() {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that changing orientation hides the menu.
     */
    /*
    @SmallTest
    @Feature({"Browser", "Main"})
    */
    @DisabledTest(message = "crbug.com/458193")
    public void testChangingOrientationHidesMenu() {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
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
        getInstrumentation().waitForIdleSync();
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
        assertTrue(mAppMenu.isShowing());
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
        getInstrumentation().waitForIdleSync();
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
