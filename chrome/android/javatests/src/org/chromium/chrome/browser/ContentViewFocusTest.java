// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.View.OnFocusChangeListener;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeEventFilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.ArrayDeque;

/**
 * Test suite for ContentView focus and its interaction with Tab switcher,
 * Tab swiping, etc.
 */
public class ContentViewFocusTest extends ChromeTabbedActivityTestBase {

    private static final int WAIT_RESPONSE_MS = 2000;

    private final ArrayDeque<Boolean> mFocusChanges = new ArrayDeque<Boolean>();

    private void addFocusChangedListener(View view) {
        view.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                synchronized (mFocusChanges) {
                    mFocusChanges.add(Boolean.valueOf(hasFocus));
                    mFocusChanges.notify();
                }
            }
        });
    }

    private boolean blockForFocusChanged() throws InterruptedException  {
        long endTime = System.currentTimeMillis() + WAIT_RESPONSE_MS * 2;
        synchronized (mFocusChanges) {
            while (true) {
                if (!mFocusChanges.isEmpty()) {
                    return mFocusChanges.removeFirst();
                }
                long sleepTime = endTime - System.currentTimeMillis();
                if (sleepTime <= 0) {
                    throw new RuntimeException("Didn't get event");
                }
                mFocusChanges.wait(sleepTime);
            }
        }
    }

    private boolean haveFocusChanges() {
        synchronized (mFocusChanges) {
            return !mFocusChanges.isEmpty();
        }
    }

    /**
     * Verify ContentView loses/gains focus on swiping tab.
     *
     * @throws Exception
     * @MediumTest
     * @Feature({"TabContents"})
     * @Restriction(RESTRICTION_TYPE_PHONE)
     * Bug: http://crbug.com/172473
     */
    @FlakyTest
    public void testHideSelectionOnPhoneTabSwiping() throws Exception {
        // Setup
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 2);
        String url = UrlUtils.getIsolatedTestFileUrl(
                "chrome/test/data/android/content_view_focus/content_view_focus_long_text.html");
        loadUrl(url);
        View view = getActivity().getActivityTab().getContentViewCore().getContainerView();

        // Give the content view focus
        TestTouchUtils.longClickView(getInstrumentation(), view, 50, 10);
        assertTrue("ContentView is focused", view.hasFocus());

        // Start the swipe
        addFocusChangedListener(view);
        final EdgeSwipeHandler edgeSwipeHandler =
                getActivity().getLayoutManager().getTopSwipeHandler();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                edgeSwipeHandler.swipeStarted(ScrollDirection.RIGHT, 0, 0);
                edgeSwipeHandler.swipeUpdated(100, 0, 100, 0, 100, 0);
            }
        });

        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria("Layout still requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver = getActivity().getLayoutManager();
                        return !driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // End the drag
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                edgeSwipeHandler.swipeFinished();
            }
        });

        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria("Layout not requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver = getActivity().getLayoutManager();
                        return driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        assertTrue("Content view didn't regain focus", blockForFocusChanged());
        assertFalse("Unexpected focus change", haveFocusChanges());
    }

    /**
     * Verify ContentView loses/gains focus on overview mode.
     *
     * @throws Exception
     * @Feature({"TabContents"})
     */
    @MediumTest
    @Feature({"TabContents"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testHideSelectionOnPhoneTabSwitcher() throws Exception {
        // Setup
        OverviewModeBehaviorWatcher showWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        OverviewModeBehaviorWatcher hideWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), false, true);
        View currentView = getActivity().getActivityTab().getContentViewCore().getContainerView();
        addFocusChangedListener(currentView);

        // Enter the tab switcher
        View tabSwitcherButton = getActivity().findViewById(R.id.tab_switcher_button);
        assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        singleClickView(getActivity().findViewById(R.id.tab_switcher_button));
        showWatcher.waitForBehavior();

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // Hide the tab switcher
        tabSwitcherButton = getActivity().findViewById(R.id.tab_switcher_button);
        assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        singleClickView(getActivity().findViewById(R.id.tab_switcher_button));
        hideWatcher.waitForBehavior();

        assertTrue("Content view didn't regain focus", blockForFocusChanged());
        assertFalse("Unexpected focus change", haveFocusChanges());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
