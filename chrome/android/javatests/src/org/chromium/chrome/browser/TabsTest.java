// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.Debug;
import android.os.SystemClock;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeEventFilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.toolbar.ToolbarPhone;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * General Tab tests.
 */
public class TabsTest extends ChromeTabbedActivityTestBase {

    private static final String TEST_FILE_PATH =
            "/chrome/test/data/android/tabstest/tabs_test.html";
    private static final String TEST_PAGE_FILE_PATH = "/chrome/test/data/google/google.html";

    private EmbeddedTestServer mTestServer;

    private float mPxToDp = 1.0f;
    private float mTabsViewHeightDp;
    private float mTabsViewWidthDp;

    private boolean mNotifyChangedCalled;

    private static final int SWIPE_TO_RIGHT_DIRECTION = 1;
    private static final int SWIPE_TO_LEFT_DIRECTION = -1;

    private static final long WAIT_RESIZE_TIMEOUT_MS = 3000;

    private static final int STRESSFUL_TAB_COUNT = 100;

    private static final String INITIAL_SIZE_TEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\" content=\"width=device-width\">"
            + "<script>"
            + "  document.writeln(window.innerWidth + ',' + window.innerHeight);"
            + "</script></head>"
            + "<body>"
            + "</body></html>");

    private static final String RESIZE_TEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>"
            + "  var resizeHappened = false;"
            + "  function onResize() {"
            + "    resizeHappened = true;"
            + "    document.getElementById('test').textContent ="
            + "       window.innerWidth + 'x' + window.innerHeight;"
            + "  }"
            + "</script></head>"
            + "<body onresize=\"onResize()\">"
            + "  <div id=\"test\">No resize event has been received yet.</div>"
            + "</body></html>");

    @Override
    protected void tearDown() throws Exception {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
        super.tearDown();
    }

    /**
     * Verify that spawning a popup from a background tab in a different model works properly.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @LargeTest
    @Feature({"Navigation"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContentSwitches.DISABLE_POPUP_BLOCKING)
    @RetryOnFailure
    public void testSpawnPopupOnBackgroundTab() throws InterruptedException, TimeoutException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        loadUrl(mTestServer.getURL(TEST_FILE_PATH));
        final Tab tab = getActivity().getActivityTab();

        newIncognitoTabFromMenu();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.getWebContents().evaluateJavaScriptForTests(
                        "(function() {"
                        + "  window.open('www.google.com');"
                        + "})()",
                        null);
            }
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getActivity().getTabModelSelector().getModel(false).getCount();
            }
        }));
    }

    @MediumTest
    @RetryOnFailure
    public void testAlertDialogDoesNotChangeActiveModel() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        newIncognitoTabFromMenu();
        loadUrl(mTestServer.getURL(TEST_FILE_PATH));
        final Tab tab = getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.getWebContents().evaluateJavaScriptForTests(
                        "(function() {"
                        + "  alert('hi');"
                        + "})()",
                        null);
            }
        });

        final AtomicReference<JavascriptAppModalDialog> dialog =
                new AtomicReference<>();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                dialog.set(JavascriptAppModalDialog.getCurrentDialogForTest());

                return dialog.get() != null;
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                dialog.get().onClick(null, DialogInterface.BUTTON_POSITIVE);
            }
        });

        dialog.set(null);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return JavascriptAppModalDialog.getCurrentDialogForTest() == null;
            }
        });

        assertTrue("Incognito model was not selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Verify New Tab Open and Close Event not from the context menu.
     * @throws InterruptedException
     *
     * https://crbug.com/490473
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     * @Restriction(RESTRICTION_TYPE_PHONE)
     */
    @DisabledTest
    public void testOpenAndCloseNewTabButton() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        startMainActivityWithURL(mTestServer.getURL(TEST_FILE_PATH));
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                String title = getActivity().getCurrentTabModel().getTabAt(0).getTitle();
                assertEquals("Data file for TabsTest", title);
            }
        });
        final int tabCount = getActivity().getCurrentTabModel().getCount();
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        View tabSwitcherButton = getActivity().findViewById(R.id.tab_switcher_button);
        assertNotNull("'tab_switcher_button' view is not found", tabSwitcherButton);
        singleClickView(tabSwitcherButton);
        overviewModeWatcher.waitForBehavior();
        overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), false, true);
        View newTabButton = getActivity().findViewById(R.id.new_tab_button);
        assertNotNull("'new_tab_button' view is not found", newTabButton);
        singleClickView(newTabButton);
        overviewModeWatcher.waitForBehavior();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertEquals("The tab count is wrong",
                        tabCount + 1, getActivity().getCurrentTabModel().getCount());
            }
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = getActivity().getCurrentTabModel().getTabAt(1);
                String title = tab.getTitle().toLowerCase(Locale.US);
                String expectedTitle = "new tab";
                return title.startsWith(expectedTitle);
            }
        });

        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertEquals(tabCount, getActivity().getCurrentTabModel().getCount());
            }
        });
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("expected keyboard show: " + show);
                return show
                        == org.chromium.ui.UiUtils.isKeyboardShowing(
                                   getActivity(), getActivity().getTabsView());
            }
        });
    }

    /**
     * Verify that opening a new tab, switching to an existing tab and closing current tab hide
     * keyboard.
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testHideKeyboard() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());

        // Open a new tab(The 1st tab) and click node.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        assertEquals("Failed to click node.", true,
                DOMUtils.clickNode(
                        getActivity().getActivityTab().getContentViewCore(), "input_text"));
        assertWaitForKeyboardStatus(true);

        // Open a new tab(the 2nd tab).
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        assertWaitForKeyboardStatus(false);

        // Click node in the 2nd tab.
        DOMUtils.clickNode(getActivity().getActivityTab().getContentViewCore(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Switch to the 1st tab.
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 1);
        assertWaitForKeyboardStatus(false);

        // Click node in the 1st tab.
        DOMUtils.clickNode(getActivity().getActivityTab().getContentViewCore(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Close current tab(the 1st tab).
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
        assertWaitForKeyboardStatus(false);
    }

    /**
     * Verify that opening a new window hides keyboard.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testHideKeyboardWhenOpeningWindow() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        // Open a new tab and click an editable node.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        assertEquals("Failed to click textarea.", true,
                DOMUtils.clickNode(
                        getActivity().getActivityTab().getContentViewCore(), "textarea"));
        assertWaitForKeyboardStatus(true);

        // Click the button to open a new window.
        assertEquals("Failed to click button.", true,
                DOMUtils.clickNode(getActivity().getActivityTab().getContentViewCore(), "button"));
        assertWaitForKeyboardStatus(false);
    }

    private void assertWaitForSelectedText(final String text) throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final String actualText =
                        getActivity().getActivityTab().getContentViewCore().getSelectedText();
                updateFailureReason(
                        "expected selected text: [" + text + "], but got: [" + actualText + "]");
                return text.equals(actualText);
            }
        });
    }

    /**
     * Generate a fling sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void fling(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        long downTime = SystemClock.uptimeMillis();
        dragStart(dragStartX, dragStartY, downTime);
        dragTo(dragStartX, dragEndX, dragStartY, dragEndY, stepCount, downTime);
        dragEnd(dragEndX, dragEndY, downTime);
    }

    private void scrollDown() {
        fling(0.f, 0.5f, 0.f, 0.75f, 100);
    }

    /**
     * Verify that the selection is collapsed when switching to the tab-switcher mode then switching
     * back. https://crbug.com/697756
     */
    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testTabSwitcherCollapseSelection() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        DOMUtils.longPressNode(getActivity().getActivityTab().getContentViewCore(), "textarea");
        assertWaitForSelectedText("helloworld");

        // Switch to tab-switcher mode, switch back, and scroll page.
        showOverviewAndWaitForAnimation();
        hideOverviewAndWaitForAnimation();
        scrollDown();
        assertWaitForSelectedText("");
    }

    /**
     * Verify that opening a new tab and navigating immediately sets a size on the newly created
     * renderer. https://crbug.com/434477.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @SmallTest
    @RetryOnFailure
    public void testNewTabSetsContentViewSize() throws InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();

        // Make sure we're on the NTP
        Tab tab = getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        loadUrl(INITIAL_SIZE_TEST_URL);

        final WebContents webContents = tab.getWebContents();
        String innerText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, "document.body.innerText").replace("\"", "");

        DisplayMetrics metrics = getActivity().getResources().getDisplayMetrics();

        // For non-integer pixel ratios like the N7v1 (1.333...), the layout system will actually
        // ceil the width.
        int expectedWidth = (int) Math.ceil(metrics.widthPixels / metrics.density);

        String[] nums = innerText.split(",");
        assertTrue(nums.length == 2);
        int innerWidth = Integer.parseInt(nums[0]);
        int innerHeight = Integer.parseInt(nums[1]);

        assertEquals(expectedWidth, innerWidth);

        // Height can be affected by browser controls so just make sure it's non-0.
        assertTrue("innerHeight was not set by page load time", innerHeight > 0);
    }

    static class SimulateClickOnMainThread implements Runnable {
        private final LayoutManagerChrome mLayoutManager;
        private final float mX;
        private final float mY;

        public SimulateClickOnMainThread(LayoutManagerChrome layoutManager, float x, float y) {
            mLayoutManager = layoutManager;
            mX = x;
            mY = y;
        }

        @Override
        public void run() {
            mLayoutManager.simulateClick(mX, mY);
        }
    }

    static class SimulateTabSwipeOnMainThread implements Runnable {
        private final LayoutManagerChrome mLayoutManager;
        private final float mX;
        private final float mY;
        private final float mDeltaX;
        private final float mDeltaY;

        public SimulateTabSwipeOnMainThread(LayoutManagerChrome layoutManager, float x, float y,
                float dX, float dY) {
            mLayoutManager = layoutManager;
            mX = x;
            mY = y;
            mDeltaX = dX;
            mDeltaY = dY;
        }

        @Override
        public void run() {
            mLayoutManager.simulateDrag(mX, mY, mDeltaX, mDeltaY);
        }
    }

    /**
     * Verify that the provided click position closes a tab.
     * @throws InterruptedException
     */
    private void checkCloseTabAtPosition(final float x, final float y)
            throws InterruptedException {
        getActivity();

        int initialTabCount = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        View button = getActivity().findViewById(R.id.tab_switcher_button);
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        singleClickView(button);
        overviewModeWatcher.waitForBehavior();
        assertTrue("Expected: " + (initialTabCount + 1) + " tab Got: "
                + getActivity().getCurrentTabModel().getCount(),
                (initialTabCount + 1) == getActivity().getCurrentTabModel().getCount());
        getInstrumentation().waitForIdleSync();
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        ChromeTabUtils.closeTabWithAction(getInstrumentation(), getActivity(), new Runnable() {
            @Override
            public void run() {
                getInstrumentation().runOnMainSync(
                        new SimulateClickOnMainThread(layoutManager, x, y));
            }
        });
        assertTrue("Expected: " + initialTabCount + " tab Got: "
                + getActivity().getCurrentTabModel().getCount(),
                initialTabCount == getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Verify close button works in the TabSwitcher in portrait mode.
     * This code does not handle properly different screen densities.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testTabSwitcherPortraitCloseButton() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        int portraitWidth = Math.min(getActivity().getResources().getDisplayMetrics().widthPixels,
                getActivity().getResources().getDisplayMetrics().heightPixels);
        // Hard-coded coordinates of the close button on the top right of the screen.
        // If the coordinates need to be updated, the easiest is to take a screenshot and measure.
        // Note that starting from the right of the screen should cover any screen size.
        checkCloseTabAtPosition(portraitWidth * mPxToDp - 32, 70);
    }

    /**
     * Verify close button works in the TabSwitcher in landscape mode.
     * This code does not handle properly different screen densities.
     * @throws InterruptedException
     * @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @FlakyTest(message = "crbug.com/170179")
    public void testTabSwitcherLandscapeCloseButton() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        // Hard-coded coordinates of the close button on the bottom left of the screen.
        // If the coordinates need to be updated, the easiest is to take a screenshot and measure.
        checkCloseTabAtPosition(31 * mPxToDp, 31 * mPxToDp);
    }

    /**
     * Verify that we can open a large number of tabs without running out of
     * memory. This test waits for the NTP to load before opening the next one.
     * This is a LargeTest but because we're doing it "slowly", we need to further scale
     * the timeout for adb am instrument and the various events.
     */
    /*
     * @EnormousTest
     * @TimeoutScale(10)
     * @Feature({"Android-TabSwitcher"})
     * Bug crbug.com/166208
     */
    @DisabledTest
    public void testOpenManyTabsSlowly() throws InterruptedException {
        int startCount = getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
            getInstrumentation().waitForIdleSync();
            assertEquals(startCount + i, getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a large number of tabs without running out of
     * memory. This test hammers the "new tab" button quickly to stress the app.
     *
     * @LargeTest
     * @TimeoutScale(10)
     * @Feature({"Android-TabSwitcher"})
     *
     */
    @FlakyTest
    public void testOpenManyTabsQuickly() {
        int startCount = getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            MenuUtils.invokeCustomMenuActionSync(getInstrumentation(), getActivity(),
                    R.id.new_tab_menu_id);
            assertEquals(startCount + i, getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a burst of new tabs, even when there are already
     * a large number of tabs open.
     * Bug: crbug.com/180718
     * @EnormousTest
     * @TimeoutScale(30)
     * @Feature({"Navigation"})
     */
    @FlakyTest
    public void testOpenManyTabsInBursts() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        final int burstSize = 5;
        final String url = mTestServer.getURL(TEST_PAGE_FILE_PATH);
        final int startCount = getActivity().getCurrentTabModel().getCount();
        for (int tabCount = startCount; tabCount < STRESSFUL_TAB_COUNT; tabCount += burstSize)  {
            loadUrlInManyNewTabs(url, burstSize);
            assertEquals(tabCount + burstSize, getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify opening 10 tabs at once and that each tab loads when selected.
     */
    /*
     * @EnormousTest
     * @TimeoutScale(30)
     * @Feature({"Navigation"})
     */
    @FlakyTest(message = "crbug.com/223110")
    public void testOpenManyTabsAtOnce10() throws InterruptedException {
        openAndVerifyManyTestTabs(10);
    }

    /**
     * Verify that we can open a large number of tabs all at once and that each
     * tab loads when selected.
     */
    private void openAndVerifyManyTestTabs(final int num) throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        final String url = mTestServer.getURL(TEST_PAGE_FILE_PATH);
        int startCount = getActivity().getCurrentTabModel().getCount();
        loadUrlInManyNewTabs(url, num);
        assertEquals(startCount + num,
                     getActivity().getCurrentTabModel().getCount());
    }

    class ClickOptionButtonOnMainThread implements Runnable {
        @Override
        public void run() {
            // This is equivalent to clickById(R.id.tab_switcher_button) but does not rely on the
            // event pipeline.
            View button = getActivity().findViewById(R.id.tab_switcher_button);
            assertNotNull("Could not find view R.id.tab_switcher_button", button);
            View toolbar = getActivity().findViewById(R.id.toolbar);
            assertNotNull("Could not find view R.id.toolbar", toolbar);
            assertTrue("R.id.toolbar is not a ToolbarPhone", toolbar instanceof ToolbarPhone);
            ((ToolbarPhone) toolbar).onClick(button);
        }
    }

    /**
     * Displays the tabSwitcher mode and wait for it to settle.
     */
    private void showOverviewAndWaitForAnimation() throws InterruptedException {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        // For some unknown reasons calling clickById(R.id.tab_switcher_button) sometimes hang.
        // The following is verbose but more reliable.
        getInstrumentation().runOnMainSync(new ClickOptionButtonOnMainThread());
        overviewModeWatcher.waitForBehavior();
    }

    /**
     * Exits the tabSwitcher mode and wait for it to settle.
     */
    private void hideOverviewAndWaitForAnimation() throws InterruptedException {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), false, true);
        getInstrumentation().runOnMainSync(new ClickOptionButtonOnMainThread());
        overviewModeWatcher.waitForBehavior();
    }

    /**
     * Opens tabs to populate the model to a given count.
     * @param targetTabCount The desired number of tabs in the model.
     * @param waitToLoad     Whether the tabs need to be fully loaded.
     * @return               The new number of tabs in the model.
     * @throws InterruptedException
     */
    private int openTabs(final int targetTabCount, boolean waitToLoad) throws InterruptedException {
        int tabCount = getActivity().getCurrentTabModel().getCount();
        while (tabCount < targetTabCount) {
            ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
            tabCount++;
            assertEquals("The tab count is wrong",
                    tabCount, getActivity().getCurrentTabModel().getCount());
            Tab tab = TabModelUtils.getCurrentTab(getActivity().getCurrentTabModel());
            while (waitToLoad && tab.isLoading()) {
                Thread.yield();
            }
        }
        return tabCount;
    }

    /**
     * Verifies that when more than 9 tabs are open only at most 8 are drawn. Basically it verifies
     * that the tab culling mechanism works properly.
     */
    /*
       @LargeTest
       @Feature({"Android-TabSwitcher"})
    */
    @DisabledTest(message = "crbug.com/156746")
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabsCulling() throws InterruptedException {
        // Open one more tabs than maxTabsDrawn.
        final int maxTabsDrawn = 8;
        int tabCount = openTabs(maxTabsDrawn + 1, false);
        showOverviewAndWaitForAnimation();

        // Check counts.
        LayoutManagerChromePhone layoutManager =
                (LayoutManagerChromePhone) getActivity().getLayoutManager();
        int drawnCount = layoutManager.getOverviewLayout().getLayoutTabsToRender().length;
        int drawnExpected = Math.min(tabCount, maxTabsDrawn);
        assertEquals("The number of drawn tab is wrong", drawnExpected, drawnCount);
    }

    /**
     * Checks the stacked tabs in the stack are visible.
     * @throws InterruptedException
     */
    private void checkTabsStacking() throws InterruptedException {
        final int count = getActivity().getCurrentTabModel().getCount();
        assertEquals("The number of tab in the stack should match the number of tabs in the model",
                count, getLayoutTabInStackCount(false));

        assertTrue("The selected tab should always be visible",
                stackTabIsVisible(false, getActivity().getCurrentTabModel().index()));
        for (int i = 0; i < Stack.MAX_NUMBER_OF_STACKED_TABS_TOP && i < count; i++) {
            assertTrue("The stacked tab " + i + " from the top should always be visible",
                    stackTabIsVisible(false, i));
        }
        for (int i = 0; i < Stack.MAX_NUMBER_OF_STACKED_TABS_BOTTOM && i < count; i++) {
            assertTrue("The stacked tab " + i + " from the bottom should always be visible",
                    stackTabIsVisible(false, count - 1 - i));
        }
    }

    /**
     * Verifies that the tab are actually stacking at the bottom and top of the screen.
     */
    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @FlakyTest(message = "crbug.com/170179")
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabsStacking() throws InterruptedException {
        final int count = openTabs(12, false);

        // Selecting the first tab to scroll all the way to the top.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), 0);
            }
        });
        showOverviewAndWaitForAnimation();
        checkTabsStacking();

        // Selecting the last tab to scroll all the way to the bottom.
        hideOverviewAndWaitForAnimation();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), count - 1);
            }
        });
        showOverviewAndWaitForAnimation();
        checkTabsStacking();
    }

    /**
     * @return A stable read of allocated size (native + dalvik) after gc.
     */
    @SuppressFBWarnings("DM_GC")
    private long getStableAllocatedSize() {
        // Measure the equivalent of allocated size native + dalvik in:
        // adb shell dumpsys meminfo | grep chrome -A 20
        int maxTries = 8;
        int tries = 0;
        long threshold = 512; // bytes
        long lastAllocatedSize = Long.MAX_VALUE;
        long currentAllocatedSize = 0;
        while (tries < maxTries && Math.abs(currentAllocatedSize - lastAllocatedSize) > threshold) {
            System.gc();
            try {
                Thread.sleep(1000 + tries * 500); // Memory measurement is not an exact science...
                lastAllocatedSize = currentAllocatedSize;
                currentAllocatedSize = Debug.getNativeHeapAllocatedSize()
                        + Runtime.getRuntime().totalMemory();
                //Log.w("MEMORY_MEASURE", "[" + tries + "/" + maxTries + "]" +
                //        "currentAllocatedSize " + currentAllocatedSize);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            tries++;
        }
        assertTrue("Could not have a stable read on native allocated size even after "
                + tries + " gc.", tries < maxTries);
        return currentAllocatedSize;
    }

    /**
     * Verify that switching back and forth to the tabswitcher does not leak memory.
     */
    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @FlakyTest(message = "crbug.com/303319")
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testTabSwitcherMemoryLeak() throws InterruptedException {
        openTabs(4, true);

        int maxTries = 10;
        int tries = 0;
        long threshold = 1024; // bytes
        long lastAllocatedSize = 0;
        long currentAllocatedSize = 2 * threshold;
        while (tries < maxTries && (lastAllocatedSize + threshold) < currentAllocatedSize) {
            showOverviewAndWaitForAnimation();

            lastAllocatedSize = currentAllocatedSize;
            currentAllocatedSize = getStableAllocatedSize();
            //Log.w("MEMORY_TEST", "[" + tries + "/" + maxTries + "]" +
            //        "currentAllocatedSize " + currentAllocatedSize);

            hideOverviewAndWaitForAnimation();
            tries++;
        }

        assertTrue("Native heap allocated size keeps increasing even after "
                + tries + " iterations", tries < maxTries);
    }

    /**
     * Verify that switching back and forth stay stable. This test last for at least 8 seconds.
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testTabSwitcherStability() throws InterruptedException {
        openTabs(8, true);

        // This is about as fast as you can ever click.
        final long fastestUserInput = 20; // ms
        for (int i = 0; i < 200; i++) {
            // Show overview
            getInstrumentation().runOnMainSync(new ClickOptionButtonOnMainThread());
            Thread.sleep(fastestUserInput);

            // hide overview
            getInstrumentation().runOnMainSync(new ClickOptionButtonOnMainThread());
            Thread.sleep(fastestUserInput);
        }
    }

    @LargeTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabSelectionPortrait() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        checkTabSelection(2, 0, false);

        // Ensure all tabs following the selected tab are off the screen when the animation is
        // complete.
        final int count = getLayoutTabInStackCount(false);
        for (int i = 1; i < count; i++) {
            float y = getLayoutTabInStackXY(false, i)[1];
            assertTrue(
                    String.format(Locale.US,
                            "Tab %d's final draw Y, %f, should exceed the view height, %f.",
                            i, y, mTabsViewHeightDp),
                    y >= mTabsViewHeightDp);
        }
    }

    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @FlakyTest(message = "crbug.com/170179")
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabSelectionLandscape() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        checkTabSelection(2, 0, true);

        // Ensure all tabs following the selected tab are off the screen when the animation is
        // complete.
        final int count = getLayoutTabInStackCount(false);
        for (int i = 1; i < count; i++) {
            float x = getLayoutTabInStackXY(false, i)[0];
            assertTrue(
                    String.format(Locale.US,
                            "Tab %d's final draw X, %f, should exceed the view width, %f.",
                            i, x, mTabsViewWidthDp),
                    x >= mTabsViewWidthDp);
        }
    }

    /**
     * Verify that we don't crash and show the overview mode after closing the last tab.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testCloseLastTabFromMain() throws InterruptedException {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        overviewModeWatcher.waitForBehavior();
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return getActivity().getLayoutManager();
    }

    private Stack getStack(final LayoutManagerChrome layoutManager, boolean isIncognito) {
        assertTrue("getStack must be executed on the ui thread",
                ThreadUtils.runningOnUiThread());
        LayoutManagerChromePhone layoutManagerPhone = (LayoutManagerChromePhone) layoutManager;
        StackLayout layout = (StackLayout) layoutManagerPhone.getOverviewLayout();
        return (layout).getTabStack(isIncognito);
    }

    private int getLayoutTabInStackCount(final boolean isIncognito) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final int[] count = new int[1];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Stack stack = getStack(layoutManager, isIncognito);
                count[0] = stack.getTabs().length;
            }
        });
        return count[0];
    }

    private boolean stackTabIsVisible(final boolean isIncognito, final int index) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final boolean[] isVisible = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Stack stack = getStack(layoutManager, isIncognito);
                isVisible[0] = (stack.getTabs())[index].getLayoutTab().isVisible();
            }
        });
        return isVisible[0];
    }

    private float[] getLayoutTabInStackXY(final boolean isIncognito, final int index) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] xy = new float[2];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Stack stack = getStack(layoutManager, isIncognito);
                xy[0] = (stack.getTabs())[index].getLayoutTab().getX();
                xy[1] = (stack.getTabs())[index].getLayoutTab().getY();
            }
        });
        return xy;
    }

    private float[] getStackTabClickTarget(final int tabIndexToSelect, final boolean isIncognito,
            final boolean isLandscape) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] target = new float[2];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Stack stack = getStack(layoutManager, isIncognito);
                StackTab[] tabs = stack.getTabs();
                // The position of the click is expressed from the top left corner of the content.
                // The aim is to find an offset that is inside the content but not on the close
                // button.  For this, we calculate the center of the visible tab area.
                LayoutTab layoutTab = tabs[tabIndexToSelect].getLayoutTab();
                LayoutTab nextLayoutTab = (tabIndexToSelect + 1) < tabs.length
                        ? tabs[tabIndexToSelect + 1].getLayoutTab() : null;

                float tabOffsetX = layoutTab.getX();
                float tabOffsetY = layoutTab.getY();
                float tabRightX, tabBottomY;
                if (isLandscape) {
                    tabRightX = nextLayoutTab != null
                            ? nextLayoutTab.getX()
                            : tabOffsetX + layoutTab.getScaledContentWidth();
                    tabBottomY = tabOffsetY + layoutTab.getScaledContentHeight();
                } else {
                    tabRightX = tabOffsetX + layoutTab.getScaledContentWidth();
                    tabBottomY = nextLayoutTab != null
                            ? nextLayoutTab.getY()
                            : tabOffsetY + layoutTab.getScaledContentHeight();
                }
                tabRightX = Math.min(tabRightX, mTabsViewWidthDp);
                tabBottomY = Math.min(tabBottomY, mTabsViewHeightDp);

                target[0] = (tabOffsetX + tabRightX) / 2.0f;
                target[1] = (tabOffsetY + tabBottomY) / 2.0f;
            }
        });
        return target;
    }

    private void checkTabSelection(int additionalTabsToOpen, int tabIndexToSelect,
            boolean isLandscape) throws InterruptedException {
        for (int i = 0; i < additionalTabsToOpen; i++) {
            ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        }
        assertEquals("Number of open tabs does not match",
                additionalTabsToOpen + 1 , getActivity().getCurrentTabModel().getCount());
        showOverviewAndWaitForAnimation();

        float[] coordinates = getStackTabClickTarget(tabIndexToSelect, false, isLandscape);
        float clickX = coordinates[0];
        float clickY = coordinates[1];

        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), false, true);

        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        getInstrumentation().runOnMainSync(new SimulateClickOnMainThread(layoutManager,
                (int) clickX, (int) clickY));
        overviewModeWatcher.waitForBehavior();

        // Make sure we did not accidentally close a tab.
        assertEquals("Number of open tabs does not match",
                additionalTabsToOpen + 1 , getActivity().getCurrentTabModel().getCount());
    }

    public void swipeToCloseTab(final int tabIndexToClose, final boolean isLandscape,
            final boolean isIncognito, final int swipeDirection) throws InterruptedException {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        float[] coordinates = getStackTabClickTarget(tabIndexToClose, isIncognito, isLandscape);
        final float clickX = coordinates[0];
        final float clickY = coordinates[1];
        Log.v("ChromeTest", String.format("clickX %f clickY %f", clickX, clickY));

        ChromeTabUtils.closeTabWithAction(getInstrumentation(), getActivity(), new Runnable() {
            @Override
            public void run() {
                if (isLandscape) {
                    getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(
                            layoutManager, clickX, clickY, 0, swipeDirection * mTabsViewWidthDp));
                } else {
                    getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(
                            layoutManager, clickX, clickY, swipeDirection * mTabsViewHeightDp, 0));
                }
            }
        });

        CriteriaHelper.pollUiThread(new Criteria("Did not finish animation") {
            @Override
            public boolean isSatisfied() {
                Layout layout = getActivity().getLayoutManager().getActiveLayout();
                return !layout.isLayoutAnimating();
            }
        });
    }

    private void swipeToCloseNTabs(int number, boolean isLandscape, boolean isIncognito,
            int swipeDirection)
            throws InterruptedException {

        for (int i = number - 1; i >= 0; i--) {
            swipeToCloseTab(i, isLandscape, isIncognito, swipeDirection);
        }
    }

    /**
     * Test closing few tabs by swiping them in Overview portrait mode.
     */
    @MediumTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher", "Main"})
    @RetryOnFailure
    public void testCloseTabPortrait() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        startMainActivityWithURL(mTestServer.getURL("/chrome/test/data/android/test.html"));

        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        int tabCount = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 3);
        assertEquals("wrong count after new tabs", tabCount + 3,
                getActivity().getCurrentTabModel().getCount());

        showOverviewAndWaitForAnimation();
        swipeToCloseNTabs(3, false, false, SWIPE_TO_LEFT_DIRECTION);

        assertEquals("Wrong tab counts after closing a few of them",
                tabCount, getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Test closing few tabs by swiping them in Overview landscape mode.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher", "Main"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @RetryOnFailure
    public void testCloseTabLandscape() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        startMainActivityWithURL(mTestServer.getURL("/chrome/test/data/android/test.html"));

        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        int tabCount = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 3);
        assertEquals("wrong count after new tabs", tabCount + 3,
                getActivity().getCurrentTabModel().getCount());

        showOverviewAndWaitForAnimation();
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);

        assertEquals("Wrong tab counts after closing a few of them",
                tabCount, getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Test close Incognito tab by swiping in Overview Portrait mode.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @RetryOnFailure
    public void testCloseIncognitoTabPortrait() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        newIncognitoTabsFromMenu(2);

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        swipeToCloseNTabs(2, false, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test close 5 Incognito tabs by swiping in Overview Portrait mode.
     */
    @Feature({"Android-TabSwitcher"})
    @MediumTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @RetryOnFailure
    public void testCloseFiveIncognitoTabPortrait() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        newIncognitoTabsFromMenu(5);

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        swipeToCloseNTabs(5, false, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Simple swipe gesture should not close tabs when two Tabstacks are open in Overview mode.
     * Test in Portrait Mode.
     */
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testSwitchTabStackWithoutClosingTabsInPortrait() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        newIncognitoTabFromMenu();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        final int normalTabCount = getLayoutTabInStackCount(false);
        final int incognitoTabCount = getLayoutTabInStackCount(true);

        LayoutManagerChrome layoutManager = updateTabsViewSize();

        // Swipe to Incognito Tabs.
        getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(layoutManager,
                mTabsViewWidthDp - 20 , mTabsViewHeightDp / 2 ,
                SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp, 0));
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue("Tabs Stack should have been changed to incognito.",
                getActivity().getCurrentTabModel().isIncognito());
        assertEquals("Normal tabs count should be unchanged while switching to incognito tabs.",
                normalTabCount, getLayoutTabInStackCount(false));

        // Swipe to regular Tabs.
        getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(layoutManager,
                20, mTabsViewHeightDp / 2,
                SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp, 0));
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals("Incognito tabs count should be unchanged while switching back to normal "
                + "tab stack.", incognitoTabCount, getLayoutTabInStackCount(true));
        assertFalse("Tabs Stack should have been changed to regular tabs.",
                getActivity().getCurrentTabModel().isIncognito());
        assertEquals("Normal tabs count should be unchanged while switching back to normal tabs.",
                normalTabCount, getLayoutTabInStackCount(false));
    }

    /**
     * Simple swipe gesture should not close tabs when two Tabstacks are open in Overview mode.
     * Test in Landscape Mode.
     */
    /*
        @MediumTest
        @Feature({"Android-TabSwitcher"})
     */
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @DisabledTest(message = "crbug.com/157259")
    public void testSwitchTabStackWithoutClosingTabsInLandscape() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        newIncognitoTabFromMenu();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        final int normalTabCount = getLayoutTabInStackCount(false);
        final int incognitoTabCount = getLayoutTabInStackCount(true);

        LayoutManagerChrome layoutManager = updateTabsViewSize();

        // Swipe to Incognito Tabs.
        getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(layoutManager,
                mTabsViewWidthDp / 2 , mTabsViewHeightDp - 20 ,
                0, SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp));
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue("Tabs Stack should have been changed to incognito.",
                getActivity().getCurrentTabModel().isIncognito());
        assertEquals("Normal tabs count should be unchanged while switching to incognito tabs.",
                normalTabCount, getLayoutTabInStackCount(false));

        // Swipe to regular Tabs.
        getInstrumentation().runOnMainSync(new SimulateTabSwipeOnMainThread(layoutManager,
                mTabsViewWidthDp / 2, 20,
                0, SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp));
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals("Incognito tabs count should be unchanged while switching back to normal "
                + "tab stack.", incognitoTabCount, getLayoutTabInStackCount(true));
        assertFalse("Tabs Stack should have been changed to regular tabs.",
                getActivity().getCurrentTabModel().isIncognito());
        assertEquals("Normal tabs count should be unchanged while switching back to normal tabs.",
                normalTabCount, getLayoutTabInStackCount(false));
    }

    /**
     * Test close Incognito tab by swiping in Overview Landscape mode.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @RetryOnFailure
    public void testCloseIncognitoTabLandscape() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        newIncognitoTabFromMenu();

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        swipeToCloseTab(0, true, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test close 5 Incognito tabs by swiping in Overview Landscape mode.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @RetryOnFailure
    public void testCloseFiveIncognitoTabLandscape() throws InterruptedException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        newIncognitoTabsFromMenu(5);

        showOverviewAndWaitForAnimation();
        UiUtils.settleDownUI(getInstrumentation());
        swipeToCloseNTabs(5, true, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test that we can safely close a tab during a fling (http://b/issue?id=5364043)
     */
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testCloseTabDuringFling() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        loadUrlInNewTab(mTestServer.getURL(
                "/chrome/test/data/android/tabstest/text_page.html"));
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ContentViewCore view = getActivity().getActivityTab().getContentViewCore();
                view.flingViewport(SystemClock.uptimeMillis(), 0, -2000);
            }
        });
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
    }

    /**
     * Flaky on instrumentation-yakju-clankium-ics. See https://crbug.com/431296.
     * @Restriction(RESTRICTION_TYPE_PHONE)
     * @MediumTest
     * @Feature({"Android-TabSwitcher"})
     */
    @FlakyTest
    public void testQuickSwitchBetweenTabAndSwitcherMode() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        final String[] urls = {
                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/three.html")};

        for (String url : urls) {
            loadUrlInNewTab(url);
        }

        int lastUrlIndex = urls.length - 1;

        View button = getActivity().findViewById(R.id.tab_switcher_button);
        assertNotNull("Could not find 'tab_switcher_button'", button);

        for (int i = 0; i < 15; i++) {
            singleClickView(button);
            // Switch back to the tab view from the tab-switcher mode.
            singleClickView(button);

            assertEquals("URL mismatch after switching back to the tab from tab-switch mode",
                    urls[lastUrlIndex], getActivity().getActivityTab().getUrl());
        }
    }

    /**
     * Open an incognito tab from menu and verify its property.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testOpenIncognitoTab() throws InterruptedException {
        newIncognitoTabFromMenu();

        assertTrue("Current Tab should be an incognito tab.",
                getActivity().getActivityTab().isIncognito());
    }

    /**
     * Test NewTab button on the browser toolbar.
     * Restricted to phones due crbug.com/429671.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testNewTabButton() throws InterruptedException {
        MenuUtils.invokeCustomMenuActionSync(getInstrumentation(), getActivity(),
                R.id.close_all_tabs_menu_id);
        UiUtils.settleDownUI(getInstrumentation());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Should be in overview mode") {
            @Override
            public boolean isSatisfied() {
                return getActivity().isInOverviewMode();
            }
        });

        int initialTabCount = getActivity().getCurrentTabModel().getCount();
        assertEquals("Tab count is expected to be 0 after closing all the tabs",
                0, initialTabCount);

        ChromeTabUtils.clickNewTabButton(this, this);

        int newTabCount = getActivity().getCurrentTabModel().getCount();
        assertEquals("Tab count is expected to be 1 after clicking Newtab button",
                1, newTabCount);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipeOnlyTab() throws InterruptedException {
        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 0, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0, false);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipePrevTab() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 1, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipeNextTab() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 0);
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 0, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipePrevTabNone() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 0);
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 0, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipeNextTabNone() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 1, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, false);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipeNextThenPrevTab() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 0);
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(false);

        assertEquals("Incorrect starting index", 0, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        assertEquals("Incorrect starting index", 1, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testToolbarSwipeNextThenPrevTabIncognito() throws InterruptedException {
        newIncognitoTabFromMenu();
        newIncognitoTabFromMenu();
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 0);
        UiUtils.settleDownUI(getInstrumentation());

        final TabModel tabModel = getActivity().getTabModelSelector().getModel(true);

        assertEquals("Incorrect starting index", 0, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        assertEquals("Incorrect starting index", 1, tabModel.index());
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    private void runToolbarSideSwipeTestOnCurrentModel(ScrollDirection direction, int finalIndex,
            boolean expectsSelection) throws InterruptedException {
        final CallbackHelper selectCallback = new CallbackHelper();
        final int id = getActivity().getCurrentTabModel().getTabAt(finalIndex).getId();
        final TabModelObserver observer = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                if (tab.getId() == id) selectCallback.notifyCalled();
            }
        };

        if (expectsSelection) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TabModelSelector selector = getActivity().getTabModelSelector();
                    for (TabModel tabModel : selector.getModels()) {
                        tabModel.addObserver(observer);
                    }
                }
            });
        }

        performToolbarSideSwipe(direction);
        waitForStaticLayout();
        assertEquals("Index after toolbar side swipe is incorrect", finalIndex,
                getActivity().getCurrentTabModel().index());

        if (expectsSelection) {
            try {
                selectCallback.waitForCallback(0);
            } catch (TimeoutException e) {
                fail("Tab selected event was never received");
            }
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TabModelSelector selector = getActivity().getTabModelSelector();
                    for (TabModel tabModel : selector.getModels()) {
                        tabModel.removeObserver(observer);
                    }
                }
            });
        }
    }


    private void performToolbarSideSwipe(ScrollDirection direction) {
        assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);
        final View toolbar = getActivity().findViewById(R.id.toolbar);
        int[] toolbarPos = new int[2];
        toolbar.getLocationOnScreen(toolbarPos);
        final int width = toolbar.getWidth();
        final int height = toolbar.getHeight();

        final int fromX = toolbarPos[0] + width / 2;
        final int toX = toolbarPos[0] + (direction == ScrollDirection.LEFT ? 0 : width);
        final int y = toolbarPos[1] + height / 2;
        final int stepCount = 10;

        long downTime = SystemClock.uptimeMillis();
        dragStart(fromX, y, downTime);
        dragTo(fromX, toX, y, y, stepCount, downTime);
        dragEnd(toX, y, downTime);
    }

    private void waitForStaticLayout() throws InterruptedException {
        CriteriaHelper.pollUiThread(
                new Criteria("Static Layout never selected after side swipe") {
                    @Override
                    public boolean isSatisfied() {
                        CompositorViewHolder compositorViewHolder = (CompositorViewHolder)
                                getActivity().findViewById(R.id.compositor_view_holder);
                        LayoutManager layoutManager = compositorViewHolder.getLayoutManager();

                        return layoutManager.getActiveLayout() instanceof StaticLayout;
                    }
                });
    }

    /**
     * Test that swipes and tab transitions are not causing URL bar to be focused.
     */
    @MediumTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testOSKIsNotShownDuringSwipe() throws InterruptedException {
        final View urlBar = getActivity().findViewById(R.id.url_bar);
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final EdgeSwipeHandler edgeSwipeHandler = layoutManager.getTopSwipeHandler();

        UiUtils.settleDownUI(getInstrumentation());
        getInstrumentation().runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        urlBar.requestFocus();
                    }
                });
        UiUtils.settleDownUI(getInstrumentation());

        getInstrumentation().runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        urlBar.clearFocus();
                    }
                });
        UiUtils.settleDownUI(getInstrumentation());
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        UiUtils.settleDownUI(getInstrumentation());

        assertFalse("Keyboard somehow got shown",
                org.chromium.ui.UiUtils.isKeyboardShowing(getActivity(), urlBar));

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                edgeSwipeHandler.swipeStarted(ScrollDirection.RIGHT, 0, 0);
                float swipeXChange = mTabsViewWidthDp / 2.f;
                edgeSwipeHandler.swipeUpdated(
                        swipeXChange, 0.f, swipeXChange, 0.f, swipeXChange, 0.f);
            }
        });

        CriteriaHelper.pollUiThread(
                new Criteria("Layout still requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver = getActivity().getLayoutManager();
                        return !driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertFalse("Keyboard should be hidden while swiping",
                        org.chromium.ui.UiUtils.isKeyboardShowing(getActivity(), urlBar));
                edgeSwipeHandler.swipeFinished();
            }
        });

        CriteriaHelper.pollUiThread(
                new Criteria("Layout not requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver = getActivity().getLayoutManager();
                        return driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        assertFalse("Keyboard should not be shown",
                org.chromium.ui.UiUtils.isKeyboardShowing(getActivity(), urlBar));
    }

    /**
     * Test that orientation changes cause the live tab reflow.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testOrientationChangeCausesLiveTabReflowInNormalView()
            throws InterruptedException, TimeoutException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        loadUrl(RESIZE_TEST_URL);
        final WebContents webContents =
                getActivity().getActivityTab().getWebContents();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                                          "resizeHappened = false;");
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals("onresize event wasn't received by the tab (normal view)",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    /**
     * Test that orientation changes cause the live tab reflow.
     */
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOrientationChangeCausesLiveTabReflowInTabSwitcher()
            throws InterruptedException, TimeoutException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        loadUrl(RESIZE_TEST_URL);
        final WebContents webContents =
                        getActivity().getActivityTab().getWebContents();

        showOverviewAndWaitForAnimation();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                                          "resizeHappened = false;");
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals("onresize event wasn't received by the live tab (tabswitcher, to Landscape)",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                                          "resizeHappened = false;");
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals("onresize event wasn't received by the live tab (tabswitcher, to Portrait)",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "resizeHappened", WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testLastClosedUndoableTabGetsHidden() {
        final TabModel model = getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        assertEquals("Too many tabs at startup", 1, model.getCount());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.closeTab(tab, false, false, true);
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue("Tab close is not undoable", model.isClosurePending(tab.getId()));
                assertTrue("Tab was not hidden", tab.isHidden());
            }
        });
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testLastClosedTabTriggersNotifyChangedCall() {
        final TabModel model = getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);
        final TabModelSelector selector = getActivity().getTabModelSelector();
        mNotifyChangedCalled = false;

        selector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                mNotifyChangedCalled = true;
            }
        });

        assertEquals("Too many tabs at startup", 1, model.getCount());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.closeTab(tab, false, false, true);
            }
        });

        assertTrue("notifyChanged() was not called", mNotifyChangedCalled);
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testTabsAreDestroyedOnModelDestruction() throws InterruptedException {
        startMainActivityOnBlankPage();
        final TabModelSelectorImpl selector =
                (TabModelSelectorImpl) getActivity().getTabModelSelector();
        final Tab tab = getActivity().getActivityTab();

        final AtomicBoolean webContentsDestroyCalled = new AtomicBoolean();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
            public void run() {
                @SuppressWarnings("unused") // Avoid GC of observer
                WebContentsObserver observer = new WebContentsObserver(tab.getWebContents()) {
                            @Override
                            public void destroy() {
                                super.destroy();
                                webContentsDestroyCalled.set(true);
                            }
                        };

                assertNotNull("No initial tab at startup", tab);
                assertNotNull("Tab does not have a web contents", tab.getWebContents());
                assertTrue("Tab is destroyed", tab.isInitialized());

                selector.destroy();

                assertNull("Tab still has a web contents", tab.getWebContents());
                assertFalse("Tab was not destroyed", tab.isInitialized());
            }
        });

        assertTrue("WebContentsObserver was never destroyed", webContentsDestroyCalled.get());
    }

    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @RetryOnFailure
    public void testIncognitoTabsNotRestoredAfterSwipe() throws Exception {
        newIncognitoTabFromMenu();

        File tabStateDir = TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
        TabModel normalModel = getActivity().getTabModelSelector().getModel(false);
        TabModel incognitoModel = getActivity().getTabModelSelector().getModel(true);
        File normalTabFile = new File(tabStateDir,
                TabState.getTabStateFilename(
                        normalModel.getTabAt(normalModel.getCount() - 1).getId(), false));
        File incognitoTabFile = new File(tabStateDir,
                TabState.getTabStateFilename(incognitoModel.getTabAt(0).getId(), true));

        assertFileExists(normalTabFile, true);
        assertFileExists(incognitoTabFile, true);

        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        ApplicationTestUtils.finishActivity(getActivity());

        // Activity will be started without a savedInstanceState.
        startMainActivityOnBlankPage();
        assertFileExists(normalTabFile, true);
        assertFileExists(incognitoTabFile, false);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        float dpToPx = getInstrumentation().getContext().getResources().getDisplayMetrics().density;
        mPxToDp = 1.0f / dpToPx;

        // Exclude the tests that can launch directly to a page other than the NTP.
        if (getName().equals("testOpenAndCloseNewTabButton")
                || getName().equals("testSwitchToTabThatDoesNotHaveThumbnail")
                || getName().equals("testCloseTabPortrait")
                || getName().equals("testCloseTabLandscape")
                || getName().equals("testTabsAreDestroyedOnModelDestruction")) {
            return;
        }
        startMainActivityFromLauncher();
    }

    private void assertFileExists(final File fileToCheck, final boolean expected)
            throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(
                    Criteria.equals(expected, new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            return fileToCheck.exists();
                        }
                    }));
    }
}
