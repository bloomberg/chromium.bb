// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_PHONE;
import static org.chromium.content.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.SystemClock;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.util.concurrent.TimeoutException;

// TODO(pedrosimonetti): add tests for recent regressions crbug.com/543319.

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
@CommandLineFlags.Add(ChromeSwitches.ENABLE_CONTEXTUAL_SEARCH_FOR_TESTING)
public class ContextualSearchManagerTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/contextualsearch/tap_test.html");
    private static final int TEST_TIMEOUT = 15000;

    // TODO(donnd): get these from TemplateURL once the low-priority or Contextual Search API
    // is fully supported.
    private static final String NORMAL_PRIORITY_SEARCH_ENDPOINT = "/search?";
    private static final String LOW_PRIORITY_SEARCH_ENDPOINT = "/s?";
    private static final String CONTEXTUAL_SEARCH_PREFETCH_PARAM = "&pf=c";

    private ContextualSearchManager mManager;
    private ContextualSearchFakeServer mFakeServer;
    private ContextualSearchPanelDelegate mPanelDelegate;
    private ContextualSearchSelectionController mSelectionController;
    private ContextualSearchPolicy mPolicy;
    private ActivityMonitor mActivityMonitor;

    public ContextualSearchManagerTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mManager = getActivity().getContextualSearchManager();

        if (mManager != null) {
            mPanelDelegate = mManager.getContextualSearchPanelDelegate();
            mFakeServer = new ContextualSearchFakeServer(mManager,
                    mManager.getOverlayContentDelegate(),
                    new OverlayContentProgressObserver(),
                    getActivity());

            mPanelDelegate.setOverlayPanelContentFactory(mFakeServer);
            mManager.setNetworkCommunicator(mFakeServer);
            mSelectionController = mManager.getSelectionController();
            mPolicy = ContextualSearchPolicy.getInstance(getActivity());

            mPolicy.overrideDecidedStateForTesting(true);
            resetCounters();
        }

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        mActivityMonitor = getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(TEST_PAGE);
    }

    /**
     * Simulates a click on the given node.
     * @param nodeId A string containing the node ID.
     */
    private void clickNode(String nodeId) throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        DOMUtils.clickNode(this, tab.getContentViewCore(), nodeId);
    }

    /**
     * Simulates a click on the given word node.
     * Waits for the bar to peek.
     * @param nodeId A string containing the node ID.
     */
    private void clickWordNode(String nodeId) throws InterruptedException, TimeoutException {
        clickNode(nodeId);
        waitForPanelToPeekAndAssert();
    }

    /**
     * Simulates a key press.
     * @param keycode The key's code.
     */
    private void pressKey(int keycode) {
        getInstrumentation().sendKeyDownUpSync(keycode);
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Simulates pressing back button.
     */
    private void pressBackButton() {
        pressKey(KeyEvent.KEYCODE_BACK);
    }

    /**
     * @return The selected text.
     */
    private String getSelectedText() {
        return mSelectionController.getSelectedText();
    }

    /**
     * Simulates a long-press on the given node.
     * @param nodeId A string containing the node ID.
     */
    private void longPressNode(String nodeId) throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        DOMUtils.longPressNode(this, tab.getContentViewCore(), nodeId);
        waitForPanelToPeekAndAssert();
    }

    /**
     * Posts a fake response on the Main thread.
     */
    private final class FakeResponseOnMainThread implements Runnable {

        private final boolean mIsNetworkUnavailable;
        private final int mResponseCode;
        private final String mSearchTerm;
        private final String mDisplayText;
        private final String mAlternateTerm;
        private final boolean mDoPreventPreload;
        private final int mStartAdjust;
        private final int mEndAdjust;

        public FakeResponseOnMainThread(boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText, String alternateTerm,
                boolean doPreventPreload, int startAdjust, int endAdjudst) {
            mIsNetworkUnavailable = isNetworkUnavailable;
            mResponseCode = responseCode;
            mSearchTerm = searchTerm;
            mDisplayText = displayText;
            mAlternateTerm = alternateTerm;
            mDoPreventPreload = doPreventPreload;
            mStartAdjust = startAdjust;
            mEndAdjust = endAdjudst;
        }

        @Override
        public void run() {
            mFakeServer.handleSearchTermResolutionResponse(
                    mIsNetworkUnavailable, mResponseCode, mSearchTerm, mDisplayText,
                    mAlternateTerm, mDoPreventPreload, mStartAdjust, mEndAdjust);
        }
    }

    /**
     * Fakes a server response with the parameters given and startAdjust and endAdjust equal to 0.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, boolean doPreventPreload) {
        fakeResponse(isNetworkUnavailable, responseCode, searchTerm, displayText, alternateTerm,
                doPreventPreload, 0, 0);
    }

    /**
     * Fakes a server response with the parameters given.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, boolean doPreventPreload,
            int startAdjust, int endAdjust) {
        if (mFakeServer.getSearchTermRequested() != null) {
            getInstrumentation().runOnMainSync(
                    new FakeResponseOnMainThread(isNetworkUnavailable, responseCode, searchTerm,
                            displayText, alternateTerm, doPreventPreload, startAdjust, endAdjust));
        }
    }

    private void assertContainsParameters(String searchTerm, String alternateTerm) {
        assertTrue(mFakeServer == null || mFakeServer.getSearchTermRequested() == null
                || mFakeServer.getLoadedUrl().contains(searchTerm)
                && mFakeServer.getLoadedUrl().contains(alternateTerm));
    }

    private void assertContainsNoParameters() {
        assertTrue(mFakeServer == null || mFakeServer.getLoadedUrl() == null);
    }

    private void assertSearchTermRequested() {
        assertNotNull(mFakeServer.getSearchTermRequested());
    }

    private void assertSearchTermNotRequested() {
        assertNull(mFakeServer.getSearchTermRequested());
    }

    private void assertPanelClosedOrUndefined() {
        boolean success = false;
        if (mPanelDelegate == null) {
            success = true;
        } else {
            PanelState panelState = mPanelDelegate.getPanelState();
            success = panelState == PanelState.CLOSED || panelState == PanelState.UNDEFINED;
        }
        assertTrue(success);
    }

    private void assertLoadedNoUrl() {
        assertTrue("Requested a search or preload when none was expected!",
                (mFakeServer == null || mFakeServer.getLoadedUrl() == null));
    }

    private void assertLoadedLowPriorityUrl() {
        if (mFakeServer == null) return;
        String message = "Expected a low priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        assertTrue(message, mFakeServer.getLoadedUrl() != null
                && mFakeServer.getLoadedUrl().contains(LOW_PRIORITY_SEARCH_ENDPOINT));
        assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    private void assertLoadedNormalPriorityUrl() {
        if (mFakeServer == null) return;
        String message = "Expected a normal priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        assertTrue(message, mFakeServer.getLoadedUrl() != null
                && mFakeServer.getLoadedUrl().contains(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        assertTrue("Normal priority request should not have the prefetch parameter, but did!",
                mFakeServer.getLoadedUrl() != null
                && !mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    private void assertNoSearchesLoaded() {
        assertEquals(0, mFakeServer.loadedUrlCount());
        assertLoadedNoUrl();
    }

    private void assertContentViewCoreCreated() {
        assertTrue(mFakeServer.didCreateContentView());
    }

    private void assertNoContentViewCore() {
        assertFalse(mFakeServer.didCreateContentView());
    }

    /**
     * Asserts that the tap triggered promo counter is enabled and at the specified count.
     */
    private void assertTapPromoCounterEnabledAt(int expectedCount) {
        assertTrue(mPolicy.getPromoTapCounter().isEnabled());
        assertEquals(expectedCount, mPolicy.getPromoTapCounter().getCount());
    }

    /**
     * Asserts that the tap triggered promo counter is disabled and at the specified count.
     */
    private void assertTapPromoCounterDisabledAt(int expectedCount) {
        assertFalse(mPolicy.getPromoTapCounter().isEnabled());
        assertEquals(expectedCount, mPolicy.getPromoTapCounter().getCount());
    }

    /**
     * Fakes navigation of the Content View with the given httpResult code.
     * The URL of the navigation is the one requested previously.
     * @param isFailure If the request resulted in a failure.
     */
    private void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayContentDelegate().onMainFrameNavigation(url, isFailure);
    }

    /**
     * Waits for the Search Panel (the Search Bar) to peek up from the bottom, and asserts that it
     * did peek.
     * @throws InterruptedException
     */
    private void waitForPanelToPeekAndAssert() throws InterruptedException {
        assertTrue("Search Bar did not peek.", waitForPanelToEnterState(PanelState.PEEKED));
    }

    /**
     * Waits for the Search Panel to expand, and asserts that it did expand.
     * @throws InterruptedException
     */
    private void waitForPanelToExpandAndAssert() throws InterruptedException {
        assertTrue("Search Panel did not expand.", waitForPanelToEnterState(PanelState.EXPANDED));
    }

    /**
     * Waits for the Search Panel to maximize, and asserts that it did maximize.
     * @throws InterruptedException
     */
    private void waitForPanelToMaximizeAndAssert() throws InterruptedException {
        assertTrue(
                "Search Panel did not maximize.", waitForPanelToEnterState(PanelState.MAXIMIZED));
    }

    /**
     * Waits for the Search Panel to close, and asserts that it did close.
     * @throws InterruptedException
     */
    private void waitForPanelToCloseAndAssert() throws InterruptedException {
        assertTrue("Search Panel did not close.", waitForPanelToEnterState(PanelState.CLOSED));
    }

    /**
     * Asserts that the panel was never opened.
     * @throws InterruptedException
     */
    private void assertPanelNeverOpened() throws InterruptedException {
        assertTrue(
                "Search Panel actually did open.", waitForPanelToEnterState(PanelState.UNDEFINED));
    }

    /**
     * Waits for the Search Panel to enter the given {@code PanelState}.
     * @throws InterruptedException
     */
    private boolean waitForPanelToEnterState(final PanelState state) throws InterruptedException {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mPanelDelegate != null
                        && mPanelDelegate.getPanelState() == state;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for the manager to finish processing a gesture.
     * Tells the manager that a gesture has started, and then waits for it to complete.
     * @throws InterruptedException
     */
    private void waitForGestureProcessingAndAssert() throws InterruptedException {
        assertTrue("Gesture processing did not complete.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !mSelectionController.wasAnyTapGestureDetected();
                    }
                }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL));
    }

    /**
     * Shorthand for a common sequence:
     * 1) Waits for gesture processing,
     * 2) Waits for the panel to close,
     * 3) Asserts that there is no selection and that the panel closed.
     * @throws InterruptedException
     */
    private void waitForGestureToClosePanelAndAssertNoSelection() throws InterruptedException {
        waitForGestureProcessingAndAssert();
        waitForPanelToCloseAndAssert();
        assertPanelClosedOrUndefined();
        assertNull(getSelectedText());
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     * @param text The string to wait for the selection to become.
     */
    private void waitForSelectionToBe(final String text) throws InterruptedException {
        assertTrue("Bar never showed desired text.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return TextUtils.equals(text, getSelectedText());
                    }
                }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL));
    }

    /**
     * Waits for the selection to be dissolved.
     * Use this method any time a test repeatedly establishes and dissolves a selection to ensure
     * that the selection has been completely dissolved before simulating the next selection event.
     * This is needed because the renderer's notification of a selection going away is async,
     * and a subsequent tap may think there's a current selection until it has been dissolved.
     */
    private void waitForSelectionDissolved() throws InterruptedException {
        assertTrue("Selection never dissolved.", CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mSelectionController.isSelectionEstablished();
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL));
    }

    /**
     * Waits for the panel to close and then waits for the selection to dissolve.
     */
    private void waitForPanelToCloseAndSelectionDissolved() throws InterruptedException {
        waitForPanelToCloseAndAssert();
        waitForSelectionDissolved();
    }

    /**
     * A ContentViewCore that has some methods stubbed out for testing.
     */
    private static final class StubbedContentViewCore extends ContentViewCore {
        private boolean mIsFocusedNodeEditable;

        public StubbedContentViewCore(Context context) {
            super(context);
        }

        public void setIsFocusedNodeEditableForTest(boolean isFocusedNodeEditable) {
            mIsFocusedNodeEditable = isFocusedNodeEditable;
        }

        @Override
        public boolean isFocusedNodeEditable() {
            return mIsFocusedNodeEditable;
        }
    }

    /**
     * Generate a swipe sequence from the give start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void swipe(float startX, float startY, float endX, float endY, int stepCount) {
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

    /**
     * Swipes the panel up to it's expanded size.
     */
    private void swipePanelUp() {
        swipe(0.5f, 0.95f, 0.5f, 0.55f, 1000);
    }

    /**
     * Swipes the panel up to it's maximized size.
     */
    private void swipePanelUpToTop() {
        swipe(0.5f, 0.95f, 0.5f, 0.05f, 1000);
    }

    /**
     * Scrolls the base page.
     */
    private void scrollBasePage() {
        swipe(0.f, 0.75f, 0.f, 0.7f, 100);
    }

    /**
     * Taps the base page near the top.
     */
    private void tapBasePageToClosePanel() throws InterruptedException {
        tapBasePage(0.1f, 0.1f);
        waitForPanelToCloseAndAssert();
    }

    /**
     * Taps the base page at the given x, y position.
     */
    private void tapBasePage(float x, float y) {
        View root = getActivity().getWindow().getDecorView().getRootView();
        x *= root.getWidth();
        y *= root.getHeight();
        TouchCommon.singleClickView(root, (int) x, (int) y);
    }

    /**
     * Click various places to cause the panel to show, expand, then close.
     */
    private void clickToExpandAndClosePanel() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        tapBarToExpandAndClosePanel();
        waitForSelectionDissolved();
    }

    /**
     * Tap on the peeking Bar to expand the panel, then taps on the base page to close it.
     */
    private void tapBarToExpandAndClosePanel() throws InterruptedException {
        tapPeekingBarToExpandAndAssert();
        tapBasePageToClosePanel();
    }

    /**
     * Generate a click in the panel's bar.
     * @barHeight The vertical position where the click should take place as a percentage
     *            of the screen size.
     */
    private void clickPanelBar(float barPositionVertical) {
        View root = getActivity().getWindow().getDecorView().getRootView();
        float w = root.getWidth();
        float h = root.getHeight();
        boolean landscape = w > h;
        float tapX = landscape ? w * barPositionVertical : w / 2f;
        float tapY = landscape ? h / 2f : h * barPositionVertical;

        TouchCommon.singleClickView(root, (int) tapX, (int) tapY);
    }

    /**
     * Taps the peeking bar to expand the panel
     */
    private void tapPeekingBarToExpandAndAssert() throws InterruptedException {
        clickPanelBar(0.95f);
        waitForPanelToExpandAndAssert();
    }

    /**
     * Simple sequence useful for checking if a Search Term Resolution request is sent.
     * Resets the fake server and clicks near to cause a search, then clicks far to let the panel
     * drop down (taking us back to the same state).
     */
    private void clickToTriggerSearchTermResolution()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
    }

    /**
     * Simple sequence useful for checking if a Search Request is prefetched.
     * Resets the fake server and clicks near to cause a search, fakes a server response to
     * trigger a prefetch, then clicks far to let the panel drop down
     * which takes us back to the starting state except the the fake server knows
     * if a prefetch occurred.
     */
    private void clickToTriggerPrefetch() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");
        assertSearchTermRequested();
        fakeResponse(false, 200, "States", "display-text", "alternate-term", false);
        waitForPanelToPeekAndAssert();
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
    }

    /**
     * Simple sequence to click, resolve, and prefetch.  Verifies a prefetch occurred.
     */
    private void clickToResolveAndAssertPrefetch() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();

        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeekAndAssert();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /**
     * Resets the tap counters on the UI thread.
     */
    private void resetCounters() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // The "Promo" tap counter is never reset outside of testing because it
                // is used to persistently count the number of peeks *ever* seen by the user
                // before the first open, and is then frozen in a disabled state to record that
                // value rather than being reset.
                // We reset it here to simulate a new user for our feature.
                mPolicy.getPromoTapCounter().reset();
                mPolicy.resetCounters();
            }
        });
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mPolicy.didResetCounters();
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Tests whether the contextual search panel hides when omnibox is clicked.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHidesWhenOmniboxFocused() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeekAndAssert();

        OmniboxTestUtils.toggleUrlBarFocus((UrlBar) getActivity().findViewById(R.id.url_bar), true);

        assertPanelClosedOrUndefined();
    }

    /**
     * Tests the doesContainAWord method.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testDoesContainAWord() {
        assertTrue(mSelectionController.doesContainAWord("word"));
        assertTrue(mSelectionController.doesContainAWord("word "));
        assertFalse("Emtpy string should not be considered a word!",
                mSelectionController.doesContainAWord(""));
        assertFalse("Special symbols should not be considered a word!",
                mSelectionController.doesContainAWord("@"));
        assertFalse("White space should not be considered a word",
                mSelectionController.doesContainAWord(" "));
        assertTrue(mSelectionController.doesContainAWord("Q2"));
        assertTrue(mSelectionController.doesContainAWord("123"));
    }

    /**
     * Tests the isValidSelection method.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testIsValidSelection() {
        StubbedContentViewCore stubbedCvc = new StubbedContentViewCore(
                getActivity().getBaseContext());
        assertTrue(mSelectionController.isValidSelection("valid", stubbedCvc));
        assertFalse(mSelectionController.isValidSelection(" ", stubbedCvc));
        stubbedCvc.setIsFocusedNodeEditableForTest(true);
        assertFalse(mSelectionController.isValidSelection("editable", stubbedCvc));
        stubbedCvc.setIsFocusedNodeEditableForTest(false);
        String numberString = "0123456789";
        StringBuilder longStringBuilder = new StringBuilder();
        for (int i = 0; i < 11; i++) {
            longStringBuilder.append(numberString);
        }
        assertTrue(mSelectionController.isValidSelection(numberString, stubbedCvc));
        assertFalse(mSelectionController.isValidSelection(longStringBuilder.toString(),
                stubbedCvc));
    }

    /**
     * Tests a simple Tap.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTap() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeekAndAssert();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests a simple Long-Press gesture, without opening the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongPress() throws InterruptedException, TimeoutException {
        longPressNode("states");

        assertNull(mFakeServer.getSearchTermRequested());
        waitForPanelToPeekAndAssert();
        assertLoadedNoUrl();
        assertNoContentViewCore();
    }

    /**
     * Tests swiping the overlay open, after an initial tap that activates the peeking card.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSwipeExpand() throws InterruptedException, TimeoutException {
        assertNoSearchesLoaded();
        clickWordNode("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeekAndAssert();
        swipePanelUp();
        waitForPanelToExpandAndAssert();
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial long-press that activates the peeking card,
     * followed by closing the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testLongPressSwipeExpand() throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        assertNoContentViewCore();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");

        waitForPanelToPeekAndAssert();
        assertLoadedNoUrl();
        assertNoContentViewCore();
        swipePanelUp();
        waitForPanelToExpandAndAssert();
        assertContentViewCoreCreated();
        assertLoadedNormalPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // tap the base page to close.
        tapBasePageToClosePanel();
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertNoContentViewCore();
    }

    /**
     * Tests a sequence in landscape orientation: swiping the overlay open, after an
     * initial tap that activates the peeking card.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSwipeExpandLandscape() throws InterruptedException, TimeoutException {
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        testSwipeExpand();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    /**
     * Tests tap to expand, after an initial tap to activate the peeking card.
     *
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @FlakyTest
    public void testTapExpand() throws InterruptedException, TimeoutException {
        assertNoSearchesLoaded();
        clickWordNode("states");
        assertNoContentViewCore();
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertContainsParameters("states", "alternate-term");
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertLoadedLowPriorityUrl();
        assertContentViewCoreCreated();
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // tap the base page to close.
        tapBasePageToClosePanel();
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertNoContentViewCore();
    }

    /**
     * Tests that only a single low-priority request is issued for a Tap/Open sequence.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTapCausesOneLowPriorityRequest() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        assertEquals(0, mFakeServer.loadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // When the bar opens, we should not make any additional request.
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that a failover for a prefetch request is issued after the panel is opened.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testPrefetchFailoverRequestMadeAfterOpen()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        assertEquals(0, mFakeServer.loadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // When the second request fails, we should not issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.loadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
        assertEquals(2, mFakeServer.loadedUrlCount());
    }

    /**
     * Tests a simple Tap with disable-preload set.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapDisablePreload() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertSearchTermRequested();
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", true);
        assertContainsNoParameters();
        waitForPanelToPeekAndAssert();
        assertLoadedNoUrl();
    }

    /**
     * Tests that long-press selects text, and a subsequent tap will unselect text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongPressGestureSelects() throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeekAndAssert();
        assertLoadedNoUrl();  // No load after long-press until opening panel.
        clickNode("question-mark");
        waitForGestureProcessingAndAssert();
        assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture selects the expected text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureSelects() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeekAndAssert();
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForGestureProcessingAndAssert();
        assertNull(getSelectedText());
    }

    /**
     * Tests that a Tap gesture on a special character does not select or show the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureOnSpecialCharacterDoesntSelect()
            throws InterruptedException, TimeoutException {
        clickNode("question-mark");
        waitForGestureProcessingAndAssert();
        assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture followed by scrolling clears the selection.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureFollowedByScrollClearsSelection()
            throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeekAndAssert();
        assertLoadedLowPriorityUrl();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping an invalid character doesn't select.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureFollowedByInvalidTextTapCloses()
            throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeekAndAssert();
        clickNode("question-mark");
        waitForGestureProcessingAndAssert();
        assertPanelClosedOrUndefined();
        assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping a non-text character doesn't select.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureFollowedByNonTextTap()
            throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeekAndAssert();
        clickNode("button");
        waitForGestureProcessingAndAssert();
        assertPanelClosedOrUndefined();
        assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture far away toggles selecting text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGestureFarAwayTogglesSelecting()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeekAndAssert();
        clickNode("states-far");
        waitForGestureProcessingAndAssert();
        assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        clickNode("states-far");
        waitForGestureProcessingAndAssert();
        waitForPanelToPeekAndAssert();
        assertEquals("States", getSelectedText());
    }

    /**
     * Tests that sequential Tap gestures nearby keep selecting.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapGesturesNearbyKeepSelecting()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeekAndAssert();
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        // Because sequential taps never hide the bar, we we can't wait for it to peek.
        // Instead we use clickNode (which doesn't wait) instead of clickWordNode and wait
        // for the selection to change.
        clickNode("states-near");
        waitForSelectionToBe("StatesNear");
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        clickNode("states");
        waitForSelectionToBe("States");
    }

    /**
     * Tests that a long-press gesture followed by scrolling does not clear the selection.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongPressGestureFollowedByScrollMaintainsSelection()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeekAndAssert();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        assertEquals("Intelligence", getSelectedText());
        assertLoadedNoUrl();
    }

    /**
     * Tests that a long-press gesture followed by a tap does not select.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongPressGestureFollowedByTapDoesntSelect()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeekAndAssert();
        clickWordNode("states-far");
        waitForGestureToClosePanelAndAssertNoSelection();
        assertLoadedNoUrl();
    }

    /**
     * Tests that the panel closes when its base page crashes.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testContextualSearchDismissedOnForegroundTabCrash()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeekAndAssert();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab().simulateRendererKilledForTesting(true);
            }
        });

        // Give the panelState time to change
        CriteriaHelper.pollForCriteria(new Criteria(){
            @Override
            public boolean isSatisfied() {
                PanelState panelState = mPanelDelegate.getPanelState();
                return panelState != PanelState.PEEKED;
            }
        });

        assertPanelClosedOrUndefined();
    }

    /**
     * Test the the panel does not close when some background tab crashes.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testContextualSearchNotDismissedOnBackgroundTabCrash()
            throws InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(),
                (ChromeTabbedActivity) getActivity());
        final Tab tab2 = TabModelUtils.getCurrentTab(getActivity().getCurrentTabModel());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), 0);
            }
        });
        getInstrumentation().waitForIdleSync();

        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeekAndAssert();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tab2.simulateRendererKilledForTesting(false);
            }
        });

        // Give the panelState time to change
        CriteriaHelper.pollForCriteria(new Criteria(){
            @Override
            public boolean isSatisfied() {
                PanelState panelState = mPanelDelegate.getPanelState();
                return panelState != PanelState.PEEKED;
            }
        });

        waitForPanelToPeekAndAssert();
    }

    /*
     * Test that tapping on the Search Bar before having a resolved search term does not
     * promote to a tab, and that after the resolution it does promote to a tab.
     *
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @FlakyTest
    public void testTapSearchBarPromotesToTab() throws InterruptedException, TimeoutException {
        // Tap on a word.
        clickWordNode("intelligence");
        assertSearchTermRequested();

        // Wait for the panel to peek.
        waitForPanelToPeekAndAssert();

        // Swipe Panel up and wait for it to maximize.
        swipePanelUpToTop();
        waitForPanelToMaximizeAndAssert();

        // Create an observer to track that a new tab is created.
        final CallbackHelper tabCreatedHelper = new CallbackHelper();
        int tabCreatedHelperCallCount = tabCreatedHelper.getCallCount();
        TabModelSelectorObserver observer = new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab) {
                tabCreatedHelper.notifyCalled();
            }
        };
        getActivity().getTabModelSelector().addObserver(observer);

        // Tap the Search Bar.
        clickPanelBar(0.05f);

        // The Search Term Resolution response hasn't arrived yet, so the Panel should not
        // be promoted. Therefore, we are asserting that the Panel is still maximized.
        waitForPanelToMaximizeAndAssert();

        // Get a Search Term Resolution response.
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");

        // Tap the Search Bar again.
        clickPanelBar(0.05f);

        // Now that the response has arrived, tapping on the Search Panel should promote it
        // to a Tab. Therefore, we are asserting that the Panel got closed.
        waitForPanelToCloseAndAssert();

        // Finally, make sure a tab was created.
        if (!FeatureUtilities.isDocumentMode(getInstrumentation().getContext())) {
            // TODO(donnd): figure out how to check for tab creation in Document mode.
            tabCreatedHelper.waitForCallback(tabCreatedHelperCallCount);
        }
        getActivity().getTabModelSelector().removeObserver(observer);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA role does not trigger.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapOnRoleIgnored() throws InterruptedException, TimeoutException {
        clickNode("role");
        assertPanelNeverOpened();
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA attribute does not trigger.
     * http://crbug.com/542874
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    */
    @DisabledTest
    public void testTapOnARIAIgnored() throws InterruptedException, TimeoutException {
        clickNode("aria");
        assertPanelNeverOpened();
    }

    /**
     * Tests that a Tap gesture on an element that is focusable does not trigger.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapOnFocusableIgnored() throws InterruptedException, TimeoutException {
        clickNode("focusable");
        assertPanelNeverOpened();
    }

    /**
     * Tests that taps can be resolve-limited for decided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.TAP_RESOLVE_LIMIT_FOR_DECIDED + "=2")
    public void testTapResolveLimitForDecided() throws InterruptedException, TimeoutException {
        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
        // 3rd click should not resolve.
        clickToTriggerSearchTermResolution();
        assertSearchTermNotRequested();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should resolve again.
        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
    }

    /**
     * Tests that taps can be resolve-limited for undecided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.TAP_RESOLVE_LIMIT_FOR_UNDECIDED + "=2")
    public void testTapResolveLimitForUndecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
        // 3rd click should not resolve.
        clickToTriggerSearchTermResolution();
        assertSearchTermNotRequested();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should resolve again.
        clickToTriggerSearchTermResolution();
        assertSearchTermRequested();
    }

    /**
     * Tests that taps can be preload-limited for decided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.TAP_PREFETCH_LIMIT_FOR_DECIDED + "=2")
    public void testTapPrefetchLimitForDecided() throws InterruptedException, TimeoutException {
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        // 3rd click should not preload.
        clickToTriggerPrefetch();
        assertLoadedNoUrl();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should preload again.
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that taps can be preload-limited for undecided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.TAP_PREFETCH_LIMIT_FOR_UNDECIDED + "=2")
    public void testTapPrefetchLimitForUndecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        // 3rd click should not preload.
        clickToTriggerPrefetch();
        assertLoadedNoUrl();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should preload again.
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests the tap triggered promo limit for opt-out.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({
            ContextualSearchFieldTrial.PROMO_ON_LIMITED_TAPS + "=true",
            ContextualSearchFieldTrial.TAP_TRIGGERED_PROMO_LIMIT + "=2"})
    @FlakyTest
    public void testTapTriggeredPromoLimitForOptOut()
            throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();

        // 3rd click won't peek the panel.
        clickNode("states");
        assertPanelClosedOrUndefined();
        // The Tap should not select any text either!
        assertNull(getSelectedText());

        // A long-press should still show the promo bar.
        longPressNode("states");
        waitForPanelToPeekAndAssert();

        // Expanding the panel should deactivate the limit.
        tapBarToExpandAndClosePanel();
        // Clear the long-press selection.
        clickNode("states-far");

        // Three taps should work now.
        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
        clickWordNode("states");
        clickNode("states-far");
        waitForPanelToCloseAndSelectionDissolved();
    }

    /**
     * This is a test that happens to create a separate bar without any content area!
     *
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContextualSearchFieldTrial.TAP_PREFETCH_LIMIT_FOR_DECIDED + "=2")
    @FlakyTest
    public void testDisembodiedBar() throws InterruptedException, TimeoutException {
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
        // 3rd click should not preload.
        clickToTriggerPrefetch();
        assertLoadedNoUrl();

        // Expanding the panel should reset the limit.
        swipePanelUp();
        tapBasePageToClosePanel();
        waitForPanelToCloseAndSelectionDissolved();

        // Click should preload again.
        clickToTriggerPrefetch();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests expanding the panel before the search term has resolved, verifies that nothing
     * loads until the resolve completes and that it's now a normal priority URL.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testExpandBeforeSearchTermResolution()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertNoContentViewCore();

        // Expanding before the search term resolves should not load anything.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNoUrl();

        // Once the response comes in, it should load.
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertContainsParameters("states", "alternate-term");
        assertLoadedNormalPriorityUrl();
        assertContentViewCoreCreated();
    }

    /**
     * Tests that an error from the Search Term Resolution request causes a fallback to a
     * search request for the literal selection.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSearchTermResolutionError() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertSearchTermRequested();
        fakeResponse(false, 403, "", "", "", false);
        assertLoadedNoUrl();
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
    }

    // --------------------------------------------------------------------------------------------
    // HTTP/HTTPS for Undecided/Decided users.
    // --------------------------------------------------------------------------------------------

    /**
     * Tests that HTTPS does not resolve in the opt-out model before the user accepts.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHttpsBeforeAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);
        mFakeServer.setShouldUseHttps(true);

        clickWordNode("states");
        assertLoadedLowPriorityUrl();
        assertSearchTermNotRequested();
    }

    /**
     * Tests that HTTPS does resolve in the opt-out model after the user accepts.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHttpsAfterAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);
        mFakeServer.setShouldUseHttps(true);

        clickToResolveAndAssertPrefetch();
    }

    /**
     * Tests that HTTP does resolve in the opt-out model before the user accepts.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHttpBeforeAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickToResolveAndAssertPrefetch();
    }

    /**
     * Tests that HTTP does resolve in the opt-out model after the user accepts.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHttpAfterAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);

        clickToResolveAndAssertPrefetch();
    }

    // --------------------------------------------------------------------------------------------
    // App Menu Suppression
    // --------------------------------------------------------------------------------------------

    /**
     * Simulates pressing the App Menu button.
     */
    private void pressAppMenuKey() {
        pressKey(KeyEvent.KEYCODE_MENU);
    }

    /**
     * Asserts whether the App Menu is visible.
     */
    private void assertAppMenuVisibility(final boolean isVisible) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity()
                        .getAppMenuHandler().isAppMenuShowing() == isVisible) return true;
                return false;
            }
        }));
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is expanded.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testAppMenuSuppressedWhenExpanded() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        tapPeekingBarToExpandAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        tapBasePageToClosePanel();

        pressAppMenuKey();
        assertAppMenuVisibility(true);
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is maximized.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAppMenuSuppressedWhenMaximized() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        swipePanelUpToTop();
        waitForPanelToMaximizeAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        pressBackButton();
        waitForPanelToCloseAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(true);
    }

    // --------------------------------------------------------------------------------------------
    // Promo tap count - the number of promo peeks.
    // --------------------------------------------------------------------------------------------

    /**
     * Tests the TapN-promo-limit feature, which disables the promo on tap after N taps if
     * the user has never ever opened the panel.  Once the panel is opened, this limiting-feature
     * is permanently disabled.
     *
     * This test is very similar to an existing test for this same feature, so I'm proactively
     * marking this as a FlakyTest too (since we're landing right before upstreaming).
     *
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({
            ContextualSearchFieldTrial.PROMO_ON_LIMITED_TAPS + "=true",
            ContextualSearchFieldTrial.TAP_TRIGGERED_PROMO_LIMIT + "=2"})
    @FlakyTest
    public void testPromoTapCount() throws InterruptedException, TimeoutException {
        // Note that this tests the basic underlying counter used by
        // testTapTriggeredPromoLimitForOptOut.
        // TODO(donnd): consider removing either this test or testTapTriggeredPromoLimitForOptOut.
        mPolicy.overrideDecidedStateForTesting(false);
        assertTapPromoCounterEnabledAt(0);

        // A simple Tap should change the counter.
        clickToTriggerPrefetch();
        assertTapPromoCounterEnabledAt(1);

        // Another Tap should increase the counter.
        clickToTriggerPrefetch();
        assertTapPromoCounterEnabledAt(2);

        // Now we're at the limit, a tap should be ignored.
        clickNode("states");
        waitForPanelToCloseAndSelectionDissolved();
        assertTapPromoCounterEnabledAt(2);

        // An open should disable the counter, but we need to use long-press (tap is now disabled).
        longPressNode("states-far");
        tapPeekingBarToExpandAndAssert();
        tapBasePageToClosePanel();
        assertTapPromoCounterDisabledAt(2);

        // Even though we closed the panel, the long-press selection is still there.
        // Tap on the question mark to dismiss the long-press selection.
        clickNode("question-mark");
        waitForSelectionToBe(null);

        // Now taps should work and not change the counter.
        clickToTriggerPrefetch();
        assertTapPromoCounterDisabledAt(2);
    }

    // --------------------------------------------------------------------------------------------
    // Promo open count
    // --------------------------------------------------------------------------------------------

    /**
     * Tests the promo open counter.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testPromoOpenCountForUndecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        // A simple click / resolve / prefetch sequence without open should not change the counter.
        clickToTriggerPrefetch();
        assertEquals(0, mPolicy.getPromoOpenCount());

        // An open should count.
        clickToExpandAndClosePanel();
        assertEquals(1, mPolicy.getPromoOpenCount());

        // Another open should count.
        clickToExpandAndClosePanel();
        assertEquals(2, mPolicy.getPromoOpenCount());

        // Once the user has decided, we should stop counting.
        mPolicy.overrideDecidedStateForTesting(true);
        clickToExpandAndClosePanel();
        assertEquals(2, mPolicy.getPromoOpenCount());
    }

    /**
     * Tests the promo open counter.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testPromoOpenCountForDecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);

        // An open should not count for decided users.
        clickToExpandAndClosePanel();
        assertEquals(0, mPolicy.getPromoOpenCount());
    }

    // --------------------------------------------------------------------------------------------
    // Tap count - number of taps between opens.
    // --------------------------------------------------------------------------------------------
    /**
     * Tests the counter for the number of taps between opens.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @FlakyTest
    public void testTapCount() throws InterruptedException, TimeoutException {
        assertEquals(0, mPolicy.getTapCount());

        // A simple Tap should change the counter.
        clickToTriggerPrefetch();
        assertEquals(1, mPolicy.getTapCount());

        // Another Tap should increase the counter.
        clickToTriggerPrefetch();
        assertEquals(2, mPolicy.getTapCount());

        // An open should reset the counter.
        clickToExpandAndClosePanel();
        assertEquals(0, mPolicy.getTapCount());
    }

    // --------------------------------------------------------------------------------------------
    // Calls to ContextualSearchObserver.
    // --------------------------------------------------------------------------------------------
    private static class TestContextualSearchObserver implements ContextualSearchObserver {
        public int hideCount;

        @Override
        public void onShowContextualSearch(GSAContextDisplaySelection selectionContext) {}

        @Override
        public void onHideContextualSearch() {
            hideCount++;
        }
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel via long press and then dismisses the panel by tapping on the base page.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testNotifyObserverHideAfterLongPress()
            throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        assertEquals(0, observer.hideCount);

        tapBasePageToClosePanel();
        assertEquals(1, observer.hideCount);
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel via tap and then dismisses the panel by tapping on the base page.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testNotifyObserverHideAfterTap() throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        clickWordNode("states");
        assertEquals(0, observer.hideCount);

        tapBasePageToClosePanel();
        assertEquals(1, observer.hideCount);
    }

    private void assertWaitForSelectActionBarVisible(final boolean visible)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return visible == getActivity().getActivityTab().getContentViewCore()
                        .isSelectActionBarShowing();
            }
        }));
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel via long press and then dismisses the panel by tapping copy (hide select action mode).
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testNotifyObserverHideOnClearSelectionAfterTap()
            throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        assertEquals(0, observer.hideCount);

        // Dismiss select action mode.
        final ContentViewCore contentViewCore = getActivity().getActivityTab().getContentViewCore();
        assertWaitForSelectActionBarVisible(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                contentViewCore.hideSelectActionMode();
            }
        });
        assertWaitForSelectActionBarVisible(false);

        waitForPanelToCloseAndAssert();
        assertEquals(1, observer.hideCount);
    }

    /**
     * Tests that the Contextual Search panel does not reappear when a long-press selection is
     * modified after the user has taken an action to explicitly dismiss the panel. Also tests
     * that the panel reappears when a new selection is made.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPreventHandlingCurrentSelectionModification()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeekAndAssert();

        // Dismiss the Contextual Search panel.
        scrollBasePage();
        assertPanelClosedOrUndefined();
        assertEquals("Intelligence", getSelectedText());

        // Simulate a selection change event and assert that the panel has not reappeared.
        mManager.onSelectionEvent(SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 333, 450);
        mManager.onSelectionEvent(SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 303, 450);
        assertPanelClosedOrUndefined();

        // Select a different word and assert that the panel has appeared.
        longPressNode("states-far");
        waitForPanelToPeekAndAssert();
    }

    /**
     * Tests a bunch of taps in a row.
     * We've had reliability problems with a sequence of simple taps, due to async dissolving
     * of selection bounds, so this helps prevent a regression with that.
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({ContextualSearchFieldTrial.TAP_RESOLVE_LIMIT_FOR_DECIDED + "=200",
            ContextualSearchFieldTrial.TAP_RESOLVE_LIMIT_FOR_UNDECIDED + "=200",
            ContextualSearchFieldTrial.TAP_PREFETCH_LIMIT_FOR_DECIDED + "=200",
            ContextualSearchFieldTrial.TAP_PREFETCH_LIMIT_FOR_UNDECIDED + "=200"})
    public void testTapALot() throws InterruptedException, TimeoutException {
        for (int i = 0; i < 50; i++) {
            clickToTriggerSearchTermResolution();
            waitForSelectionDissolved();
            assertSearchTermRequested();
        }
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation has a user gesture.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity());
        final NavigationParams navigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, true /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                false /* hasUserGestureCarryover */);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertFalse(
                        mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                                externalNavHandler, navigationParams));
            }
        });
        assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an initial
     * navigation has a user gesture but the redirected external navigation doesn't.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRedirectedExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity());

        final NavigationParams initialNavigationParams = new NavigationParams("http://test.com", "",
                false /* isPost */, true /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, false /* isExternalProtocol */, true /* isMainFrame */,
                false /* hasUserGestureCarryover */);
        final NavigationParams redirectedNavigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, false /* hasUserGesture */, PageTransition.LINK,
                true /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                false /* hasUserGestureCarryover */);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                OverlayContentDelegate delegate = mManager.getOverlayContentDelegate();
                assertTrue(delegate.shouldInterceptNavigation(
                        externalNavHandler, initialNavigationParams));
                assertFalse(delegate.shouldInterceptNavigation(
                        externalNavHandler, redirectedNavigationParams));
            }
        });
        assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation doesn't have a user gesture.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testExternalNavigationWithoutUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity());
        final NavigationParams navigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, false /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                false /* hasUserGestureCarryover */);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertFalse(
                        mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                                externalNavHandler, navigationParams));
            }
        });
        assertEquals(0, mActivityMonitor.getHits());
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testSelectionExpansionOnSearchTermResolution()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("intelligence");
        waitForPanelToPeekAndAssert();

        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false, -14, 0);
        waitForSelectionToBe("United States Intelligence");
    }

    /**
     * Tests that long-press triggers the Peek Promo, and expanding the Panel dismisses it.
     *
     * Re-enable the test after fixing http://crbug.com/540820.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     */
    @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @FlakyTest
    @CommandLineFlags.Add(ContextualSearchFieldTrial.PEEK_PROMO_ENABLED + "=true")
    public void testLongPressShowsPeekPromo() throws InterruptedException, TimeoutException {
        // Must be in undecided state in order to trigger the Peek Promo.
        mPolicy.overrideDecidedStateForTesting(false);
        // Must have never opened the Panel in order to trigger the Peek Promo.
        assertEquals(0, mPolicy.getPromoOpenCount());

        // Long press and make sure the Promo shows.
        longPressNode("intelligence");
        waitForPanelToPeekAndAssert();
        assertTrue(mPanelDelegate.isPeekPromoVisible());

        // After expanding the Panel the Promo should be invisible.
        swipePanelUp();
        waitForPanelToExpandAndAssert();
        assertFalse(mPanelDelegate.isPeekPromoVisible());

        // After closing the Panel the Promo should still be invisible.
        tapBasePageToClosePanel();
        assertFalse(mPanelDelegate.isPeekPromoVisible());

        // Click elsewhere to clear the selection.
        clickNode("question-mark");
        waitForSelectionToBe(null);

        // Now that the Panel was opened at least once, the Promo should not show again.
        longPressNode("intelligence");
        waitForPanelToPeekAndAssert();
        assertFalse(mPanelDelegate.isPeekPromoVisible());
    }
}
