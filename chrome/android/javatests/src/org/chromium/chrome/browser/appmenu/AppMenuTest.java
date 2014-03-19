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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellActivity.AppMenuHandlerFactory;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.R;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests AppMenu popup
 */
public class AppMenuTest extends ChromeShellTestBase {
    private AppMenu mAppMenu;
    private AppMenuHandlerForTest mAppMenuHandler;

    /**
     * AppMenuHandler that will be used to intercept item selections for testing.
     */
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

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ChromeShellActivity.setAppMenuHandlerFactory(new AppMenuHandlerFactory() {
            @Override
            public AppMenuHandler getAppMenuHandler(Activity activity,
                    AppMenuPropertiesDelegate delegate, int menuResourceId) {
                mAppMenuHandler = new AppMenuHandlerForTest(activity, delegate, menuResourceId);
                return mAppMenuHandler;
            }
        });
        launchChromeShellWithBlankPage();
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final View menuButton = getActivity().findViewById(R.id.menu_button);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                menuButton.performClick();
            }
        });
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getAppMenuHandler().isAppMenuShowing();
            }
        }));
        mAppMenu = getActivity().getAppMenuHandler().getAppMenu();
        pressKey(KeyEvent.KEYCODE_SPACE);

    }
    /**
     * Test bounds when accessing the menu through the keyboard.
     * Make sure that the menu stays open when trying to move past the first and last items.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuBoundaries() throws InterruptedException {
        moveToBoundary(false, true);
        assertEquals(mAppMenu.getCount() - 1, mAppMenu.getCurrentFocusedPosition());
        moveToBoundary(true, true);
        assertEquals(0, mAppMenu.getCurrentFocusedPosition());
        moveToBoundary(false, true);
        assertEquals(mAppMenu.getCount() - 1, mAppMenu.getCurrentFocusedPosition());
    }

    /**
     * Test that typing ENTER immediately opening the menu works.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnOpen() throws InterruptedException {
        hitEnterAndAssertItemSelected();
    }

    /**
     * Test that hitting ENTER past the top item doesn't crash Chrome.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() throws InterruptedException {
        moveToBoundary(true, true);
        assertEquals(0, mAppMenu.getCurrentFocusedPosition());
        hitEnterAndAssertItemSelected();
    }

    /**
     * Test that hitting ENTER past the bottom item doesn't crash Chrome.
     * Catches regressions for http://crbug.com/181067
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastBottomItem() throws InterruptedException {
        moveToBoundary(false, true);
        assertEquals(mAppMenu.getCount() - 1, mAppMenu.getCurrentFocusedPosition());
        hitEnterAndAssertItemSelected();
    }

    /**
     * Test that hitting ENTER on the top item actually triggers the top item.
     * Catches regressions for https://crbug.com/191239 for shrunken menus.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemLandscape() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        moveToBoundary(true, false);
        assertEquals(0, mAppMenu.getCurrentFocusedPosition());
        hitEnterAndAssertItemSelected();
    }

    /**
     * Test that hitting ENTER on the top item doesn't crash Chrome.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemPortrait() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        moveToBoundary(true, false);
        assertEquals(0, mAppMenu.getCurrentFocusedPosition());
        hitEnterAndAssertItemSelected();
    }

    private void hitEnterAndAssertItemSelected() throws InterruptedException {
        final int expectedItemId = mAppMenu.getCurrentFocusedItemId();
        pressKey(KeyEvent.KEYCODE_ENTER);
        assertTrue("Did not select a correct menu item",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mAppMenuHandler.mLastSelectedItemId == expectedItemId;
                    }
                }));
    }

    private void moveToBoundary(boolean towardsTop, boolean movePast) throws InterruptedException {
        // Move to the boundary.
        final int end = towardsTop ? 0 : mAppMenu.getCount() - 1;
        int increment = towardsTop ? -1 : 1;
        for (int index = mAppMenu.getCurrentFocusedPosition(); index != end; index += increment) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            final int expectedPosition = index + increment;
            assertTrue("Focus did not move to the next menu item",
                    CriteriaHelper.pollForCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return mAppMenu.getCurrentFocusedPosition() == expectedPosition;
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
                            return mAppMenu.getCurrentFocusedPosition() == end;
                        }
                    }));
        }

        // The menu should stay open.
        assertTrue(mAppMenu.isShowing());
    }

    private void pressKey(int keycode) {
        getInstrumentation().sendKeyDownUpSync(keycode);
        getInstrumentation().waitForIdleSync();
    }
}
