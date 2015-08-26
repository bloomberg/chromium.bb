// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListPopupWindow;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests AppMenu popup
 */
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
        mAppMenu = getActivity().getAppMenuHandler().getAppMenuForTest();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAppMenu.getPopup().getListView().setSelection(0);
            }
        });
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getCurrentFocusedRow() == 0;
            }
        }));
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Verify opening a new tab from the menu.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
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
    public void testKeyboardMenuBoundaries() throws InterruptedException {
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
    public void testKeyboardMenuEnterOnOpen() throws InterruptedException {
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the top item doesn't crash Chrome.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() throws InterruptedException {
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
    public void testKeyboardEnterAfterMovePastBottomItem() throws InterruptedException {
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
    public void testKeyboardMenuEnterOnTopItemLandscape() throws InterruptedException {
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
    public void testKeyboardMenuEnterOnTopItemPortrait() throws InterruptedException {
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
    crbug.com/458193
    */
    @DisabledTest
    public void testChangingOrientationHidesMenu() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertTrue("AppMenu did not dismiss",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !mAppMenuHandler.isAppMenuShowing();
                    }
                }));
    }

    private void showAppMenuAndAssertMenuShown() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAppMenuHandler.showAppMenu(null, false);
            }
        });
        assertTrue("AppMenu did not show",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mAppMenuHandler.isAppMenuShowing();
                    }
                }));
    }

    private void hitEnterAndAssertAppMenuDismissed() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        pressKey(KeyEvent.KEYCODE_ENTER);
        assertTrue("AppMenu did not dismiss",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !mAppMenuHandler.isAppMenuShowing();
                    }
                }));
    }

    private void moveToBoundary(boolean towardsTop, boolean movePast) throws InterruptedException {
        // Move to the boundary.
        final int end = towardsTop ? 0 : getCount() - 1;
        int increment = towardsTop ? -1 : 1;
        for (int index = getCurrentFocusedRow(); index != end; index += increment) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            final int expectedPosition = index + increment;
            assertTrue("Focus did not move to the next menu item",
                    CriteriaHelper.pollForCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return getCurrentFocusedRow() == expectedPosition;
                        }
                    }));
        }

        // Try moving past it by one.
        if (movePast) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            assertTrue("Focus moved past the edge menu item",
                    CriteriaHelper.pollForCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return getCurrentFocusedRow() == end;
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
