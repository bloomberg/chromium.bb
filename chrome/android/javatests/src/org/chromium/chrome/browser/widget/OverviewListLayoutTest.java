// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.os.SystemClock;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.widget.accessibility.AccessibilityTabModelListItem;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests accessibility UI.
 */
public class OverviewListLayoutTest extends ChromeTabbedActivityTestBase {

    private static final String PAGE_1_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%201<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";
    private static final String PAGE_2_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%202<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";

    private static final int SWIPE_START_X_OFFSET = 10;
    private static final int SWIPE_START_Y_OFFSET = 10;
    private static final int SWIPE_END_X = 20;

    private class ChildCountCriteria extends Criteria {
        private final int mChildCount;

        public ChildCountCriteria(int count) {
            mChildCount = count;
        }

        @Override
        public boolean isSatisfied() {
            return mChildCount == ThreadUtils.runOnUiThreadBlockingNoException(
                    new Callable<Integer>() {
                        @Override
                        public Integer call() {
                            return getList().getChildCount();
                        }
                    });
        }
    }

    private class TabModelCountCountCriteria extends Criteria {
        private final boolean mIncognito;
        private final int mTabCount;

        public TabModelCountCountCriteria(boolean incognito, int count) {
            mIncognito = incognito;
            mTabCount = count;
        }

        @Override
        public boolean isSatisfied() {
            int actualTabCount = ThreadUtils.runOnUiThreadBlockingNoException(
                    new Callable<Integer>() {
                        @Override
                        public Integer call() {
                            return getActivity().getTabModelSelector()
                                    .getModel(mIncognito).getCount();
                        }
                    });
            updateFailureReason("Expected tab count: " + mTabCount + ", Actual: " + actualTabCount);
            return mTabCount == actualTabCount;
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER);
        startMainActivityFromLauncher();
    }

    private ViewGroup getContainer() {
        return ((OverviewListLayout) getActivity().getOverviewListLayout()).getContainer();
    }

    private ListView getList() {
        return (ListView) getContainer().findViewById(R.id.list_view);
    }

    private void setupTabs() throws InterruptedException {
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 3);

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(4));
    }

    private AccessibilityTabModelListItem getListItemAndDisableAnimations(int index) {
        AccessibilityTabModelListItem item = getListItem(index);
        item.disableAnimations();
        return item;
    }

    private AccessibilityTabModelListItem getListItem(int index) {
        AccessibilityTabModelListItem item =
                (AccessibilityTabModelListItem) getList().getChildAt(index);
        return item;
    }

    private CharSequence getTabTitleOfListItem(int index) {
        View childView = getListItem(index);
        TextView childTextView =
                (TextView) childView.findViewById(org.chromium.chrome.R.id.tab_title);
        return childTextView.getText();
    }

    private void toggleTabSwitcher(final boolean expectVisible) throws Exception {
        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                boolean isVisible = (getContainer() != null && getContainer().getParent() != null);
                return isVisible == expectVisible;
            }
        });
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanEnterSwitcher() {
        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));

        assertNotNull("Accessibility container was not initialized", getContainer());
        assertNotNull("Accessibility container was not visible", getContainer().getParent());
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanLeaveSwitcher() {
        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));

        assertNotNull("Accessibility container was not initialized", getContainer());
        assertNotNull("Accessibility container was not visible", getContainer().getParent());

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));
        assertNull("Accessibility container was not visible", getContainer().getParent());
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanCloseWithCloseButton() throws InterruptedException, TimeoutException {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(0);

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = getActivity().getCurrentTabModel();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), item.findViewById(R.id.close_btn));

        didReceiveClosureCommittedHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Tab not closed", 3, getList().getChildCount());
            }
        });
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanSwipeClosed() throws InterruptedException, TimeoutException {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(1);

        int[] location = TestTouchUtils.getAbsoluteLocationFromRelative(
                item, SWIPE_START_X_OFFSET, SWIPE_START_Y_OFFSET);

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = getActivity().getCurrentTabModel();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        long downTime = SystemClock.uptimeMillis();
        dragStart(location[0], location[1], downTime);
        dragTo(location[0], (int) (item.getWidth() / 1.5), location[1], location[1], 20, downTime);
        dragEnd(400, location[1], downTime);

        didReceiveClosureCommittedHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Tab not closed", 3, getList().getChildCount());
            }
        });
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testResetSwipe() throws InterruptedException {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(0);

        int[] location = TestTouchUtils.getAbsoluteLocationFromRelative(
                item, SWIPE_START_X_OFFSET, SWIPE_START_Y_OFFSET);

        dragTo(location[0], SWIPE_END_X, location[1], location[1], 20, SystemClock.uptimeMillis());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Tab not reset as expected", 4, getList().getChildCount());
            }
        });
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAndUndo() throws InterruptedException, TimeoutException {
        setupTabs();

        final AccessibilityTabModelListItem item = getListItem(0);

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        final TabModel model = getActivity().getCurrentTabModel();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabPendingClosure(Tab tab) {
                didReceivePendingClosureHelper.notifyCalled();
            }
        });

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), item.findViewById(R.id.close_btn));

        didReceivePendingClosureHelper.waitForCallback(0);

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), item.findViewById(R.id.undo_button));

        assertFalse("Close not undone", item.hasPendingClosure());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Tab closure not undone", 4, getList().getChildCount());
            }
        });
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAll() throws InterruptedException {
        setupTabs();

        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.close_all_tabs_menu_id);

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(0));
        CriteriaHelper.pollForCriteria(new TabModelCountCountCriteria(false, 0));
        assertFalse(getActivity().findViewById(R.id.tab_switcher_button).isEnabled());
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAllIncognito() throws InterruptedException {
        setupTabs();
        newIncognitoTabsFromMenu(2);
        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));
        CriteriaHelper.pollForCriteria(new ChildCountCriteria(2));

        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.close_all_incognito_tabs_menu_id);
        CriteriaHelper.pollForCriteria(new TabModelCountCountCriteria(true, 0));

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(4));
        assertTrue(getActivity().findViewById(R.id.tab_switcher_button).isEnabled());

        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.close_all_tabs_menu_id);

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(0));
        CriteriaHelper.pollForCriteria(new TabModelCountCountCriteria(false, 0));
        assertFalse(getActivity().findViewById(R.id.tab_switcher_button).isEnabled());
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testModelSwitcherVisibility() throws InterruptedException {
        setupTabs();

        View switcherButtons = getContainer().findViewById(R.id.button_wrapper);

        assertEquals(
                "Tab Model Switcher buttons visible", View.GONE, switcherButtons.getVisibility());

        newIncognitoTabsFromMenu(2);

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));

        assertEquals("Tab Model switcher buttons not visible", View.VISIBLE,
                switcherButtons.getVisibility());
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testModelSwitcherFunctionality() throws InterruptedException {
        newIncognitoTabsFromMenu(2);

        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 3);

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), getActivity().findViewById(R.id.tab_switcher_button));

        View switcherButtons = getContainer().findViewById(R.id.button_wrapper);

        assertEquals("Tab Model Switcher buttons visible", View.VISIBLE,
                switcherButtons.getVisibility());

        View incognitoButton = switcherButtons.findViewById(R.id.incognito_tabs_button);

        assertNotNull("IncognitoButton is null", incognitoButton);

        TestTouchUtils.performClickOnMainSync(getInstrumentation(), incognitoButton);

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(2));

        TestTouchUtils.performClickOnMainSync(
                getInstrumentation(), switcherButtons.findViewById(R.id.standard_tabs_button));

        CriteriaHelper.pollForCriteria(new ChildCountCriteria(4));
    }

    /**
     * Tests that the TabObserver of the {@link AccessibilityTabModelListItem} is added back
     * to the Tab after the View is hidden.  This requires bringing the tab switcher back twice
     * because the TabObserver is removed/added when the tab switcher's View is detached from/
     * attached to the window.
     */
    @MediumTest
    @Feature({"Accessibility"})
    public void testObservesTitleChanges() throws Exception {
        loadUrl(PAGE_1_HTML);

        // Bring the tab switcher forward and send it away twice.
        toggleTabSwitcher(true);
        assertEquals("Page 1", getTabTitleOfListItem(0));
        toggleTabSwitcher(false);
        toggleTabSwitcher(true);
        toggleTabSwitcher(false);

        // Load another URL.
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(PAGE_2_HTML);

        // Bring the tab switcher forward and check the title.
        toggleTabSwitcher(true);
        assertEquals("Page 2", getTabTitleOfListItem(0));
    }

    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseTabThroughTabStrip() throws InterruptedException, TimeoutException {
        setupTabs();

        getListItemAndDisableAnimations(0);
        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = getActivity().getCurrentTabModel();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        StripLayoutTab tab = TabStripUtils.findStripLayoutTab(getActivity(), false,
                model.getTabAt(0).getId());
        TabStripUtils.clickCompositorButton(tab.getCloseButton(), this);
        didReceiveClosureCommittedHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Tab not closed", 3, getList().getChildCount());
            }
        });
    }
}
