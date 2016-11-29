// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.content.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Point;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchCaptionControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchIconSpriteControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchQuickActionControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeSlowResolveSearch;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

// TODO(pedrosimonetti): Create class with limited API to encapsulate the internals of simulations.
// TODO(pedrosimonetti): Separate tests into different classes grouped by type of tests. Examples:
// Gestures (Tap, LongPress), Search Term Resolution (resolves, expand selection, prevent preload,
// translation), Panel interaction (tap, fling up/down, close), Content (creation, loading,
// visibility, history, delayed load), Tab Promotion, Policy (add tests to check if policies
// affect the behavior correctly), General (remaining tests), etc.

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@CommandLineFlags.Add(ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED)
public class ContextualSearchManagerTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE =
            "/chrome/test/data/android/contextualsearch/tap_test.html";
    private static final int TEST_TIMEOUT = 15000;
    private static final int TEST_EXPECTED_FAILURE_TIMEOUT = 1000;
    private static final int PLENTY_OF_TAPS = 1000;

    // TODO(donnd): get these from TemplateURL once the low-priority or Contextual Search API
    // is fully supported.
    private static final String NORMAL_PRIORITY_SEARCH_ENDPOINT = "/search?";
    private static final String LOW_PRIORITY_SEARCH_ENDPOINT = "/s?";
    private static final String CONTEXTUAL_SEARCH_PREFETCH_PARAM = "&pf=c";
    // The number of ms to delay startup for all tests.
    private static final int ACTIVITY_STARTUP_DELAY_MS = 1000;

    private ActivityMonitor mActivityMonitor;
    private ContextualSearchFakeServer mFakeServer;
    private ContextualSearchManager mManager;
    private ContextualSearchPanel mPanel;
    private ContextualSearchPolicy mPolicy;
    private ContextualSearchSelectionController mSelectionController;
    private EmbeddedTestServer mTestServer;

    // State for an individual test.
    FakeSlowResolveSearch mLatestSlowResolveSearch;

    public ContextualSearchManagerTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        // We have to set up the test server before starting the activity.
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());

        super.setUp();

        mManager = getActivity().getContextualSearchManager();

        assertNotNull(mManager);
        mPanel = mManager.getContextualSearchPanel();

        mSelectionController = mManager.getSelectionController();
        mPolicy = mManager.getContextualSearchPolicy();
        mPolicy.overrideDecidedStateForTesting(true);
        resetCounters();

        mFakeServer = new ContextualSearchFakeServer(mPolicy, this, mManager,
                mManager.getOverlayContentDelegate(), new OverlayContentProgressObserver(),
                getActivity());

        mPanel.setOverlayPanelContentFactory(mFakeServer);
        mManager.setNetworkCommunicator(mFakeServer);

        registerFakeSearches();

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        mActivityMonitor = getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Sets the online status and reloads the current Tab with our test URL.
     * @param isOnline Whether to go online.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void setOnlineStatusAndReload(boolean isOnline)
            throws InterruptedException, TimeoutException {
        mFakeServer.setIsOnline(isOnline);
        final String testUrl = mTestServer.getURL(TEST_PAGE);
        final Tab tab = getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.reload();
            }
        });
        // Make sure the page is fully loaded.
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);
    }

    //============================================================================================
    // Public API
    //============================================================================================

    /**
     * Simulates a long-press on the given node without waiting for the panel to respond.
     * @param nodeId A string containing the node ID.
     */
    public void longPressNodeWithoutWaiting(String nodeId)
            throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        DOMUtils.longPressNode(this, tab.getContentViewCore(), nodeId);
    }

    /**
     * Simulates a long-press on the given node and waits for the panel to peek.
     * @param nodeId A string containing the node ID.
     */
    public void longPressNode(String nodeId) throws InterruptedException, TimeoutException {
        longPressNodeWithoutWaiting(nodeId);
        waitForPanelToPeek();
    }

    /**
     * Simulates a click on the given node.
     * @param nodeId A string containing the node ID.
     */
    public void clickNode(String nodeId) throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        DOMUtils.clickNode(this, tab.getContentViewCore(), nodeId);
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     * @param text The string to wait for the selection to become.
     */
    public void waitForSelectionToBe(final String text) throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(text, new Callable<String>() {
            @Override
            public String call() {
                return getSelectedText();
            }
        }), TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for the FakeTapSearch to become ready.
     * @param search A given FakeTapSearch.
     */
    public void waitForSearchTermResolutionToStart(
            final ContextualSearchFakeServer.FakeTapSearch search) throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Fake Search Term Resolution never started.") {
                    @Override
                    public boolean isSatisfied() {
                        return search.didStartSearchTermResolution();
                    }
                }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for the FakeTapSearch to become ready.
     * @param search A given FakeTapSearch.
     */
    public void waitForSearchTermResolutionToFinish(
            final ContextualSearchFakeServer.FakeTapSearch search) throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(new Criteria("Fake Search was never ready.") {
            @Override
            public boolean isSatisfied() {
                return search.didFinishSearchTermResolution();
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Runs the given Runnable in the main thread.
     * @param runnable The Runnable.
     */
    public void runOnMainSync(Runnable runnable) {
        getInstrumentation().runOnMainSync(runnable);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
        // There's a problem with immediate startup that causes flakes due to the page not being
        // ready, so specify a startup-delay of 1000 for legacy behavior.  See crbug.com/635661.
        // TODO(donnd): find a better way to wait for page-ready, or at least reduce the delay!
        Thread.sleep(ACTIVITY_STARTUP_DELAY_MS);
    }

    //============================================================================================
    // Fake Searches Helpers
    //============================================================================================

    /**
     * Simulates a long-press triggered search.
     *
     * @param nodeId The id of the node to be long-pressed.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void simulateLongPressSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        ContextualSearchFakeServer.FakeLongPressSearch search =
                mFakeServer.getFakeLongPressSearch(nodeId);
        search.simulate();
        waitForPanelToPeek();
    }

    /**
     * Simulates a tap-triggered search.
     *
     * @param nodeId The id of the node to be tapped.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void simulateTapSearch(String nodeId) throws InterruptedException, TimeoutException {
        ContextualSearchFakeServer.FakeTapSearch search = mFakeServer.getFakeTapSearch(nodeId);
        search.simulate();
        assertLoadedSearchTermMatches(search.getSearchTerm());
        waitForPanelToPeek();
    }

    /**
     * Simulates a tap-triggered search that has been limited by tap-limits.
     *
     * @param nodeId The id of the node to be tapped.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void simulateLimitedTapSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        ContextualSearchFakeServer.FakeTapSearch search = mFakeServer.getFakeTapSearch(nodeId);
        search.simulate();
        assertLoadedNoUrl();
        // Tap-limited behavior is to not resolve or preload, but will still do a literal search.
        waitForPanelToPeek();
    }

    /**
     * Simulates a tap-triggered search with slow server response.
     *
     * @param nodeId The id of the node to be tapped.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void simulateSlowResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        mLatestSlowResolveSearch = mFakeServer.getFakeSlowResolveSearch(nodeId);
        mLatestSlowResolveSearch.simulate();
        waitForPanelToPeek();
    }

    /**
     * Simulates a slow response for the most recent {@link FakeSlowResolveSearch} set up
     * by calling simulateSlowResolveSearch.
     * @throws TimeoutException
     * @throws InterruptedException
     */
    private void simulateSlowResolveFinished() throws InterruptedException, TimeoutException {
        // Allow the slow Resolution to finish, waiting for it to complete.
        mLatestSlowResolveSearch.finishResolve();
        assertLoadedSearchTermMatches(mLatestSlowResolveSearch.getSearchTerm());
    }

    /**
     * Registers all fake searches to be used in tests.
     */
    private void registerFakeSearches() {
        mFakeServer.registerFakeSearches();
    }

    //============================================================================================
    // Fake Response
    // TODO(pedrosimonetti): remove these methods and use the new infrastructure instead.
    //============================================================================================

    /**
     * Posts a fake response on the Main thread.
     */
    private final class FakeResponseOnMainThread implements Runnable {

        private final boolean mIsNetworkUnavailable;
        private final int mResponseCode;
        private final String mSearchTerm;
        private final String mDisplayText;
        private final String mAlternateTerm;
        private final String mMid;
        private final boolean mDoPreventPreload;
        private final int mStartAdjust;
        private final int mEndAdjust;
        private final String mContextLanguage;
        private final String mThumbnailUrl;
        private final String mCaption;
        private final String mQuickActionUri;
        private final int mQuickActionCategory;

        public FakeResponseOnMainThread(boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText, String alternateTerm, String mid,
                boolean doPreventPreload, int startAdjust, int endAdjudst, String contextLanguage,
                String thumbnailUrl, String caption, String quickActionUri,
                int quickActionCategory) {
            mIsNetworkUnavailable = isNetworkUnavailable;
            mResponseCode = responseCode;
            mSearchTerm = searchTerm;
            mDisplayText = displayText;
            mAlternateTerm = alternateTerm;
            mMid = mid;
            mDoPreventPreload = doPreventPreload;
            mStartAdjust = startAdjust;
            mEndAdjust = endAdjudst;
            mContextLanguage = contextLanguage;
            mThumbnailUrl = thumbnailUrl;
            mCaption = caption;
            mQuickActionUri = quickActionUri;
            mQuickActionCategory = quickActionCategory;
        }

        @Override
        public void run() {
            mFakeServer.handleSearchTermResolutionResponse(mIsNetworkUnavailable, mResponseCode,
                    mSearchTerm, mDisplayText, mAlternateTerm, mMid, mDoPreventPreload,
                    mStartAdjust, mEndAdjust, mContextLanguage, mThumbnailUrl, mCaption,
                    mQuickActionUri, mQuickActionCategory);
        }
    }

    /**
     * Fakes a server response with the parameters given and startAdjust and endAdjust equal to 0.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, boolean doPreventPreload) {
        fakeResponse(isNetworkUnavailable, responseCode, searchTerm, displayText, alternateTerm,
                null, doPreventPreload, 0, 0, "", "", "", "", QuickActionCategory.NONE);
    }

    /**
     * Fakes a server response with the parameters given.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode, String searchTerm,
            String displayText, String alternateTerm, String mid, boolean doPreventPreload,
            int startAdjust, int endAdjust, String contextLanguage, String thumbnailUrl,
            String caption, String quickActionUri, int quickActionCategory) {
        if (mFakeServer.getSearchTermRequested() != null) {
            getInstrumentation().runOnMainSync(new FakeResponseOnMainThread(isNetworkUnavailable,
                    responseCode, searchTerm, displayText, alternateTerm, mid, doPreventPreload,
                    startAdjust, endAdjust, contextLanguage, thumbnailUrl, caption,
                    quickActionUri, quickActionCategory));
        }
    }

    //============================================================================================
    // Content Helpers
    //============================================================================================

    /**
     * @return The Panel's ContentViewCore.
     */
    private ContentViewCore getPanelContentViewCore() {
        return mPanel.getContentViewCore();
    }

    /**
     * @return Whether the Panel's ContentViewCore is visible.
     */
    private boolean isContentViewCoreVisible() {
        ContextualSearchFakeServer.ContentViewCoreWrapper contentViewCore =
                (ContextualSearchFakeServer.ContentViewCoreWrapper) getPanelContentViewCore();
        return contentViewCore != null ? contentViewCore.isVisible() : false;
    }

    /**
     * Asserts that the Panel's ContentViewCore is created.
     */
    private void assertContentViewCoreCreated() {
        assertNotNull(getPanelContentViewCore());
    }

    /**
     * Asserts that the Panel's ContentViewCore is not created.
     */
    private void assertNoContentViewCore() {
        assertNull(getPanelContentViewCore());
    }

    /**
     * Asserts that the Panel's ContentViewCore is visible.
     */
    private void assertContentViewCoreVisible() {
        assertTrue(isContentViewCoreVisible());
    }

    /**
     * Asserts that the Panel's ContentViewCore onShow() method was never called.
     */
    private void assertNeverCalledContentViewCoreOnShow() {
        assertFalse(mFakeServer.didEverCallContentViewCoreOnShow());
    }

    /**
     * Asserts that the Panel's ContentViewCore is created
     */
    private void assertContentViewCoreCreatedButNeverMadeVisible() {
        assertContentViewCoreCreated();
        assertFalse(isContentViewCoreVisible());
        assertNeverCalledContentViewCoreOnShow();
    }

    /**
     * Fakes navigation of the Content View to the URL that was previously requested.
     * @param isFailure whether the request resulted in a failure.
     */
    private void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayContentDelegate().onMainFrameNavigation(url, false, isFailure);
    }

    /**
     * A ContentViewCore that has some methods stubbed out for testing.
     * TODO(pedrosimonetti): consider using the real ContentViewCore instead.
     */
    private static final class StubbedContentViewCore extends ContentViewCore {
        private boolean mIsFocusedNodeEditable;

        public StubbedContentViewCore(Context context) {
            super(context, "");
        }

        /**
         * Mocks the result of isFocusedNodeEditable() for testing.
         * @param isFocusedNodeEditable Whether the focused node is editable.
         */
        public void setIsFocusedNodeEditableForTest(boolean isFocusedNodeEditable) {
            mIsFocusedNodeEditable = isFocusedNodeEditable;
        }

        @Override
        public boolean isFocusedNodeEditable() {
            return mIsFocusedNodeEditable;
        }
    }

    //============================================================================================
    // Other Helpers
    // TODO(pedrosimonetti): organize into sections.
    //============================================================================================

    /**
     * Simulates a click on the given word node.
     * Waits for the bar to peek.
     * @param nodeId A string containing the node ID.
     */
    private void clickWordNode(String nodeId) throws InterruptedException, TimeoutException {
        clickNode(nodeId);
        waitForPanelToPeek();
    }

    /**
     * Simulates a key press.
     * @param keycode The key's code.
     */
    private void pressKey(int keycode) {
        KeyUtils.singleKeyEventActivity(getInstrumentation(), getActivity(), keycode);
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
     * Asserts that the loaded search term matches the provided value.
     * @param searchTerm The provided search term.
     */
    private void assertLoadedSearchTermMatches(String searchTerm) {
        boolean doesMatch = false;
        String loadedUrl = mFakeServer.getLoadedUrl();
        doesMatch = loadedUrl != null && loadedUrl.contains("q=" + searchTerm);
        String message = loadedUrl == null ? "but there was no loaded URL!"
                                           : "in URL: " + loadedUrl;
        assertTrue("Expected to find searchTerm '" + searchTerm + "', " + message, doesMatch);
    }

    private void assertContainsParameters(String searchTerm, String alternateTerm) {
        assertTrue(mFakeServer.getSearchTermRequested() == null
                || mFakeServer.getLoadedUrl().contains(searchTerm)
                        && mFakeServer.getLoadedUrl().contains(alternateTerm));
    }

    private void assertContainsNoParameters() {
        assertTrue(mFakeServer.getLoadedUrl() == null);
    }

    private void assertSearchTermRequested() {
        assertNotNull(mFakeServer.getSearchTermRequested());
    }

    private void assertSearchTermNotRequested() {
        assertNull(mFakeServer.getSearchTermRequested());
    }

    private void assertPanelClosedOrUndefined() {
        boolean success = false;
        if (mPanel == null) {
            success = true;
        } else {
            PanelState panelState = mPanel.getPanelState();
            success = panelState == PanelState.CLOSED || panelState == PanelState.UNDEFINED;
        }
        assertTrue(success);
    }

    private void assertPanelPeeked() {
        assertTrue(mPanel.getPanelState() == PanelState.PEEKED);
    }

    private void assertLoadedNoUrl() {
        assertTrue("Requested a search or preload when none was expected!",
                mFakeServer.getLoadedUrl() == null);
    }

    private void assertLoadedLowPriorityUrl() {
        String message = "Expected a low priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        assertTrue(message, mFakeServer.getLoadedUrl() != null
                && mFakeServer.getLoadedUrl().contains(LOW_PRIORITY_SEARCH_ENDPOINT));
        assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    private void assertLoadedNormalPriorityUrl() {
        String message = "Expected a normal priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        assertTrue(message, mFakeServer.getLoadedUrl() != null
                && mFakeServer.getLoadedUrl().contains(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        assertTrue("Normal priority request should not have the prefetch parameter, but did!",
                mFakeServer.getLoadedUrl() != null
                        && !mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    private void assertNoSearchesLoaded() {
        assertEquals(0, mFakeServer.getLoadedUrlCount());
        assertLoadedNoUrl();
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
     * Waits for the Search Panel (the Search Bar) to peek up from the bottom, and asserts that it
     * did peek.
     * @throws InterruptedException
     */
    private void waitForPanelToPeek() throws InterruptedException {
        waitForPanelToEnterState(PanelState.PEEKED);
    }

    /**
     * Waits for the Search Panel to expand, and asserts that it did expand.
     * @throws InterruptedException
     */
    private void waitForPanelToExpand() throws InterruptedException {
        waitForPanelToEnterState(PanelState.EXPANDED);
    }

    /**
     * Waits for the Search Panel to maximize, and asserts that it did maximize.
     * @throws InterruptedException
     */
    private void waitForPanelToMaximize() throws InterruptedException {
        waitForPanelToEnterState(PanelState.MAXIMIZED);
    }

    /**
     * Waits for the Search Panel to close, and asserts that it did close.
     * @throws InterruptedException
     */
    private void waitForPanelToClose() throws InterruptedException {
        waitForPanelToEnterState(PanelState.CLOSED);
    }

    /**
     * Waits for the Search Panel to enter the given {@code PanelState} and assert.
     * @param state The {@link PanelState} to wait for.
     * @throws InterruptedException
     */
    private void waitForPanelToEnterState(final PanelState state)
            throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (mPanel == null) return false;
                updateFailureReason("Panel did not enter " + state + " state. "
                        + "Instead, the current state is " + mPanel.getPanelState() + ".");
                return mPanel.getPanelState() == state;
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Asserts that the panel is still in the given state and continues to stay that way
     * for a while.
     * Waits for a reasonable amount of time for the panel to change to a different state,
     * and verifies that it did not change state while this method is executing.
     * Note that it's quite possible for the panel to transition through some other state and
     * back to the initial state before this method is called without that being detected,
     * because this method only monitors state during its own execution.
     * @param initialState The initial state of the panel at the beginning of an operation that
     *        should not change the panel state.
     * @throws InterruptedException
     */
    private void assertPanelStillInState(final PanelState initialState)
            throws InterruptedException {
        boolean didChangeState = false;
        long startTime = SystemClock.uptimeMillis();
        while (!didChangeState
                && SystemClock.uptimeMillis() - startTime < TEST_EXPECTED_FAILURE_TIMEOUT) {
            Thread.sleep(DEFAULT_POLLING_INTERVAL);
            didChangeState = mPanel.getPanelState() != initialState;
        }
        assertFalse(didChangeState);
    }

    /**
     * Waits for the manager to finish processing a gesture.
     * Tells the manager that a gesture has started, and then waits for it to complete.
     * @throws InterruptedException
     */
    private void waitForGestureProcessing() throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Gesture processing did not complete.") {
                    @Override
                    public boolean isSatisfied() {
                        return !mSelectionController.wasAnyTapGestureDetected();
                    }
                }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Shorthand for a common sequence:
     * 1) Waits for gesture processing,
     * 2) Waits for the panel to close,
     * 3) Asserts that there is no selection and that the panel closed.
     * @throws InterruptedException
     */
    private void waitForGestureToClosePanelAndAssertNoSelection() throws InterruptedException {
        waitForGestureProcessing();
        waitForPanelToClose();
        assertPanelClosedOrUndefined();
        assertNull(getSelectedText());
    }

    /**
     * Waits for the selection to be dissolved.
     * Use this method any time a test repeatedly establishes and dissolves a selection to ensure
     * that the selection has been completely dissolved before simulating the next selection event.
     * This is needed because the renderer's notification of a selection going away is async,
     * and a subsequent tap may think there's a current selection until it has been dissolved.
     */
    private void waitForSelectionDissolved() throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(new Criteria("Selection never dissolved.") {
            @Override
            public boolean isSatisfied() {
                return !mSelectionController.isSelectionEstablished();
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for the panel to close and then waits for the selection to dissolve.
     */
    private void waitForPanelToCloseAndSelectionDissolved() throws InterruptedException {
        waitForPanelToClose();
        waitForSelectionDissolved();
    }

    private void waitToPreventDoubleTapRecognition() throws InterruptedException {
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
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

    /**
     * Generate a swipe sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void swipe(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        int halfCount = stepCount / 2;
        long downTime = SystemClock.uptimeMillis();
        dragStart(dragStartX, dragStartY, downTime);
        dragTo(dragStartX, dragEndX, dragStartY, dragEndY, halfCount, downTime);
        // Generate events in the stationary end position in order to simulate a "pause" in
        // the movement, therefore preventing this gesture from being interpreted as a fling.
        dragTo(dragEndX, dragEndX, dragEndY, dragEndY, halfCount, downTime);
        dragEnd(dragEndX, dragEndY, downTime);
    }

    /**
     * Flings the panel up to its expanded state.
     */
    private void flingPanelUp() {
        // TODO(pedrosimonetti): Consider using a swipe method instead.
        fling(0.5f, 0.95f, 0.5f, 0.55f, 1000);
    }

    /**
     * Swipes the panel down to its peeked state.
     */
    private void swipePanelDown() {
        swipe(0.5f, 0.55f, 0.5f, 0.95f, 100);
    }

    /**
     * Flings the panel up to its maximized state.
     */
    private void flingPanelUpToTop() {
        // TODO(pedrosimonetti): Consider using a swipe method instead.
        fling(0.5f, 0.95f, 0.5f, 0.05f, 1000);
    }

    /**
     * Scrolls the base page.
     */
    private void scrollBasePage() {
        // TODO(pedrosimonetti): Consider using a swipe method instead.
        fling(0.f, 0.75f, 0.f, 0.7f, 100);
    }

    /**
     * Taps the base page near the top.
     */
    private void tapBasePageToClosePanel() throws InterruptedException {
        // TODO(pedrosimonetti): This is not reliable. Find a better approach.
        // We use the far right side (x == 0.9f) to prevent simulating a tap on top of an
        // existing long-press selection (the pins are a tap target). This might not work on RTL.
        // We are using y == 0.35f because otherwise it will fail for long press cases.
        // It might be better to get the position of the Panel and tap just about outside
        // the Panel. I suspect some Flaky tests are caused by this problem (ones involving
        // long press and trying to close with the bar peeking, with a long press selection
        // established).
        tapBasePage(0.9f, 0.35f);
        waitForPanelToClose();
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
     * TODO(donnd): Replace this method with panelBarClick since this appears to be unreliable.
     * @barHeight The vertical position where the click should take place as a percentage
     *            of the screen size.
     */
    private void clickPanelBar(float barPositionVertical) {
        View root = getActivity().getWindow().getDecorView().getRootView();
        float w = root.getWidth();
        float h = root.getHeight();
        float tapX = w / 2f;
        float tapY = h * barPositionVertical;

        TouchCommon.singleClickView(root, (int) tapX, (int) tapY);
    }

    /**
     * Taps the peeking bar to expand the panel
     */
    private void tapPeekingBarToExpandAndAssert() throws InterruptedException {
        clickPanelBar(0.95f);
        waitForPanelToExpand();
    }

    /**
     * Simple sequence useful for checking if a Search Request is prefetched.
     * Resets the fake server and clicks near to cause a search, then closes the panel,
     * which takes us back to the starting state except that the fake server knows
     * if a prefetch occurred.
     */
    private void clickToTriggerPrefetch() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        simulateTapSearch("search");
        waitForPanelToPeek();
        closePanel();
        waitForPanelToCloseAndSelectionDissolved();
    }

    /**
     * Simple sequence useful for checking that a search is not prefetched because it has
     * hit the tap limit.
     * Resets the fake server and clicks near to cause a search, then closes the panel,
     * which takes us back to the starting state except that the fake server knows
     * if a prefetch occurred.
     */
    private void clickToTriggerLimitedPrefetch() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        simulateLimitedTapSearch("search");
        waitForPanelToPeek();
        closePanel();
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
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /**
     * Resets all the counters used, by resetting all shared preferences.
     */
    private void resetCounters() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContextUtils.getAppSharedPreferences().edit().clear().apply();
            }
        });
    }

    /**
     * Force the Panel to handle a click in the Bar.
     * @throws InterruptedException
     */
    private void forcePanelToHandleBarClick() throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // TODO(donnd): provide better time and x,y data to make this more broadly useful.
                mPanel.handleBarClick(0, 0, 0);
            }
        });
    }

    /**
     * Force the Panel to close.
     * @throws InterruptedException
     */
    private void closePanel() throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mPanel.closePanel(StateChangeReason.UNKNOWN, false);
            }
        });
    }

    /**
     * Waits for the Action Bar to be visible in response to a selection.
     */
    private void waitForSelectActionBarVisible() throws InterruptedException {
        assertWaitForSelectActionBarVisible(true);
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests whether the contextual search panel hides when omnibox is clicked.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testHidesWhenOmniboxFocused() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();

        OmniboxTestUtils.toggleUrlBarFocus((UrlBar) getActivity().findViewById(R.id.url_bar), true);

        assertPanelClosedOrUndefined();
    }

    /**
     * Tests the doesContainAWord method.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
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
    @RetryOnFailure
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
    @RetryOnFailure
    public void testTap() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests a simple Long-Press gesture, without opening the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testLongPress() throws InterruptedException, TimeoutException {
        longPressNode("states");

        assertNull(mFakeServer.getSearchTermRequested());
        waitForPanelToPeek();
        assertLoadedNoUrl();
        assertNoContentViewCore();
    }

    /**
     * Tests swiping the overlay open, after an initial tap that activates the peeking card.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testSwipeExpand() throws InterruptedException, TimeoutException {
        assertNoSearchesLoaded();
        clickWordNode("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeek();
        flingPanelUp();
        waitForPanelToExpand();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial long-press that activates the peeking card,
     * followed by closing the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testLongPressSwipeExpand() throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        assertNoContentViewCore();

        // TODO(pedrosimonetti): Long press does not resolve so we shouldn't be faking one.
        // Consider changing the fake server to create a fake response automatically,
        // when one is requested by the Manager.

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");

        waitForPanelToPeek();
        assertLoadedNoUrl();
        assertNoContentViewCore();
        flingPanelUp();
        waitForPanelToExpand();
        assertContentViewCoreCreated();
        assertLoadedNormalPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // tap the base page to close.
        tapBasePageToClosePanel();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertNoContentViewCore();
    }

    /**
     * Tests that only a single low-priority request is issued for a Tap/Open sequence.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCausesOneLowPriorityRequest() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the bar opens, we should not make any additional request.
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that a failover for a prefetch request is issued after the panel is opened.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testPrefetchFailoverRequestMadeAfterOpen()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request fails, we should not issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
        assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests a simple Tap with disable-preload set.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapDisablePreload() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertSearchTermRequested();
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", true);
        assertContainsNoParameters();
        waitForPanelToPeek();
        assertLoadedNoUrl();
    }

    /**
     * Tests that long-press selects text, and a subsequent tap will unselect text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testLongPressGestureSelects() throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedNoUrl();  // No load after long-press until opening panel.
        clickNode("question-mark");
        waitForGestureProcessing();
        waitForPanelToCloseAndSelectionDissolved();
        assertNull(getSelectedText());
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture selects the expected text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapGestureSelects() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForGestureProcessing();
        assertNull(getSelectedText());
    }

    /**
     * Tests that a Tap gesture on a special character does not select or show the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapGestureOnSpecialCharacterDoesntSelect()
            throws InterruptedException, TimeoutException {
        clickNode("question-mark");
        waitForGestureProcessing();
        assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture followed by scrolling clears the selection.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapGestureFollowedByScrollClearsSelection()
            throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
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
    @RetryOnFailure
    public void testTapGestureFollowedByInvalidTextTapCloses()
            throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("question-mark");
        waitForGestureProcessing();
        assertPanelClosedOrUndefined();
        assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping a non-text character doesn't select.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     * @RetryOnFailure
     * crbug.com/665633
     */
    @DisabledTest
    public void testTapGestureFollowedByNonTextTap() throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("button");
        waitForPanelToCloseAndSelectionDissolved();
    }

    /**
     * Tests that a Tap gesture far away toggles selecting text.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapGestureFarAwayTogglesSelecting()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        clickNode("states-far");
        waitForGestureProcessing();
        assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        clickNode("states-far");
        waitForGestureProcessing();
        waitForPanelToPeek();
        assertEquals("States", getSelectedText());
    }

    /**
     * Tests that sequential Tap gestures nearby keep selecting.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapGesturesNearbyKeepSelecting() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeek();
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
    public void testLongPressGestureFollowedByScrollMaintainsSelection()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeek();
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
    public void testLongPressGestureFollowedByTapDoesntSelect()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeek();
        clickWordNode("states-far");
        waitForGestureToClosePanelAndAssertNoSelection();
        assertLoadedNoUrl();
    }

    /**
     * Tests that the panel closes when its base page crashes.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testContextualSearchDismissedOnForegroundTabCrash()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeek();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab().simulateRendererKilledForTesting(true);
            }
        });

        // Give the panelState time to change
        CriteriaHelper.pollInstrumentationThread(new Criteria(){
            @Override
            public boolean isSatisfied() {
                PanelState panelState = mPanel.getPanelState();
                return panelState != PanelState.PEEKED;
            }
        });

        assertPanelClosedOrUndefined();
    }

    /**
     * Test the the panel does not close when some background tab crashes.
     */
    @SmallTest
    @Feature({"ContextualSearch"})

    @RetryOnFailure
    public void testContextualSearchNotDismissedOnBackgroundTabCrash()
            throws InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(),
                (ChromeTabbedActivity) getActivity());
        final Tab tab2 = TabModelUtils.getCurrentTab(getActivity().getCurrentTabModel());

        // TODO(donnd): consider using runOnUiThreadBlocking, won't need to waitForIdleSync?
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), 0);
            }
        });
        getInstrumentation().waitForIdleSync();

        clickWordNode("states");
        assertEquals("States", getSelectedText());
        waitForPanelToPeek();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tab2.simulateRendererKilledForTesting(false);
            }
        });

        waitForPanelToPeek();
    }

    /*
     * Test that tapping on the Search Bar before having a resolved search term does not
     * promote to a tab, and that after the resolution it does promote to a tab.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapSearchBarPromotesToTab() throws InterruptedException, TimeoutException {
        // -------- SET UP ---------
        // Track Tab creation with this helper.
        final CallbackHelper tabCreatedHelper = new CallbackHelper();
        int tabCreatedHelperCallCount = tabCreatedHelper.getCallCount();
        TabModelSelectorObserver observer = new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab) {
                tabCreatedHelper.notifyCalled();
            }
        };
        getActivity().getTabModelSelector().addObserver(observer);

        // -------- TEST ---------
        // Start a slow-resolve search and maximize the Panel.
        simulateSlowResolveSearch("search");
        flingPanelUpToTop();
        waitForPanelToMaximize();

        // A click in the Bar should not promote since we are still waiting to Resolve.
        forcePanelToHandleBarClick();

        // Assert that the Panel is still maximized.
        waitForPanelToMaximize();

        // Let the Search Term Resolution finish.
        simulateSlowResolveFinished();

        // Now a click in the Bar should promote to a separate tab.
        forcePanelToHandleBarClick();

        // The Panel should now be closed.
        waitForPanelToClose();

        // Make sure a tab was created.
        tabCreatedHelper.waitForCallback(tabCreatedHelperCallCount);

        // -------- CLEAN UP ---------
        getActivity().getTabModelSelector().removeObserver(observer);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA role does not trigger.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapOnRoleIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("role");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA attribute does not trigger.
     * http://crbug.com/542874
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapOnARIAIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("aria");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element that is focusable does not trigger.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapOnFocusableIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("focusable");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that taps can be resolve and prefetch limited for decided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    @RetryOnFailure
    public void testTapLimitForDecided() throws InterruptedException, TimeoutException {
        mPolicy.setTapLimitForDecidedForTesting(2);
        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
        // 3rd click should not resolve or prefetch.
        clickToTriggerLimitedPrefetch();
        assertSearchTermNotRequested();
        assertLoadedNoUrl();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should resolve and prefetch again.
        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that taps can be resolve-limited for undecided users.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    @RetryOnFailure
    public void testTapLimitForUndecided() throws InterruptedException, TimeoutException {
        mPolicy.setTapLimitForUndecidedForTesting(2);
        mPolicy.overrideDecidedStateForTesting(false);

        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
        // 3rd click should not resolve or prefetch.
        clickToTriggerLimitedPrefetch();
        assertSearchTermNotRequested();
        assertLoadedNoUrl();

        // Expanding the panel should reset the limit.
        clickToExpandAndClosePanel();

        // Click should resolve and prefetch again.
        clickToTriggerPrefetch();
        assertSearchTermRequested();
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests expanding the panel before the search term has resolved, verifies that nothing
     * loads until the resolve completes and that it's now a normal priority URL.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
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
        assertContentViewCoreVisible();
    }

    /**
     * Tests that an error from the Search Term Resolution request causes a fallback to a
     * search request for the literal selection.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
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
    @RetryOnFailure
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
    @RetryOnFailure
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
    @RetryOnFailure
    public void testHttpBeforeAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickToResolveAndAssertPrefetch();
    }

    /**
     * Tests that HTTP does resolve in the opt-out model after the user accepts.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
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
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(isVisible, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return getActivity().getAppMenuHandler().isAppMenuShowing();
                    }
                }));
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is expanded.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
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
    @RetryOnFailure
    public void testAppMenuSuppressedWhenMaximized() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        flingPanelUpToTop();
        waitForPanelToMaximize();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        pressBackButton();
        waitForPanelToClose();

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
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testPromoTapCount() throws InterruptedException, TimeoutException {
        mPolicy.setPromoTapTriggeredLimitForTesting(2);
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
        pressBackButton();
        waitForPanelToClose();
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    @RetryOnFailure
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserverHideAfterTap() throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        clickWordNode("states");
        assertEquals(0, observer.hideCount);

        tapBasePageToClosePanel();
        assertEquals(1, observer.hideCount);
    }

    /**
     * Asserts that the action bar does or does not become visible in response to a selection.
     * @param visible Whether the Action Bar must become visible or not.
     * @throws InterruptedException
     */
    private void assertWaitForSelectActionBarVisible(final boolean visible)
            throws InterruptedException {
        CriteriaHelper.pollUiThread(Criteria.equals(visible, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return getActivity().getActivityTab().getContentViewCore()
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
    @RetryOnFailure
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
                contentViewCore.destroySelectActionMode();
            }
        });
        assertWaitForSelectActionBarVisible(false);

        waitForPanelToClose();
        assertEquals(1, observer.hideCount);
    }

    /**
     * Tests that the Contextual Search panel does not reappear when a long-press selection is
     * modified after the user has taken an action to explicitly dismiss the panel. Also tests
     * that the panel reappears when a new selection is made.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testPreventHandlingCurrentSelectionModification()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeek();

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
        waitForPanelToPeek();
    }

    /**
     * Tests a bunch of taps in a row.
     * We've had reliability problems with a sequence of simple taps, due to async dissolving
     * of selection bounds, so this helps prevent a regression with that.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.SUPPRESSION_TAPS + "=" + PLENTY_OF_TAPS)
    @RetryOnFailure
    public void testTapALot() throws InterruptedException, TimeoutException {
        mPolicy.setTapLimitForDecidedForTesting(PLENTY_OF_TAPS);
        mPolicy.setTapLimitForUndecidedForTesting(PLENTY_OF_TAPS);
        for (int i = 0; i < 50; i++) {
            clickToTriggerPrefetch();
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
    public void testExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity().getActivityTab());
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
    @RetryOnFailure
    public void testRedirectedExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity().getActivityTab());

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
    @RetryOnFailure
    public void testExternalNavigationWithoutUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(getActivity().getActivityTab());
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
    @RetryOnFailure
    public void testSelectionExpansionOnSearchTermResolution()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("intelligence");
        waitForPanelToPeek();

        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                null, false, -14, 0, "", "", "", "", QuickActionCategory.NONE);
        waitForSelectionToBe("United States Intelligence");
    }

    /**
     * Tests that long-press triggers the Peek Promo, and expanding the Panel dismisses it.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags
            .Add(ContextualSearchFieldTrial.PEEK_PROMO_ENABLED + "=true")
            @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
            @RetryOnFailure
            public void testLongPressShowsPeekPromo()
            throws InterruptedException, TimeoutException {
        // Must be in undecided state in order to trigger the Peek Promo.
        mPolicy.overrideDecidedStateForTesting(false);
        // Must have never opened the Panel in order to trigger the Peek Promo.
        assertEquals(0, mPolicy.getPromoOpenCount());

        // Long press and make sure the Promo shows.
        longPressNode("intelligence");
        waitForPanelToPeek();
        assertTrue(mPanel.isPeekPromoVisible());

        // After expanding the Panel the Promo should be invisible.
        flingPanelUp();
        waitForPanelToExpand();
        assertFalse(mPanel.isPeekPromoVisible());

        // After closing the Panel the Promo should still be invisible.
        tapBasePageToClosePanel();
        assertFalse(mPanel.isPeekPromoVisible());

        // Click elsewhere to clear the selection.
        clickNode("question-mark");
        waitForSelectionToBe(null);

        // Now that the Panel was opened at least once, the Promo should not show again.
        longPressNode("intelligence");
        waitForPanelToPeek();
        assertFalse(mPanel.isPeekPromoVisible());
    }

    //============================================================================================
    // Content Tests
    //============================================================================================

    /**
     * Tests that tap followed by expand makes Content visible.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testTapContentVisibility() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
    }

    /**
     * Tests that long press followed by expand creates Content and makes it visible.
     *
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testLongPressContentVisibility() throws InterruptedException, TimeoutException {
        // Simulate a long press and make sure no Content is created.
        simulateLongPressSearch("search");
        assertNoContentViewCore();
        assertNoSearchesLoaded();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
    }

    /**
     * Tests swiping panel up and down after a tap search will only load the Content once.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapMultipleSwipeOnlyLoadsContentOnce()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests swiping panel up and down after a long press search will only load the Content once.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testLongPressMultipleSwipeOnlyLoadsContentOnce()
            throws InterruptedException, TimeoutException {
        // Simulate a long press and make sure no Content is created.
        simulateLongPressSearch("search");
        assertNoContentViewCore();
        assertNoSearchesLoaded();

        // Expanding the Panel should load the URL and make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained tap searches create new Content.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testChainedSearchCreatesNewContent() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        waitToPreventDoubleTapRecognition();

        // Simulate a new tap and make sure a new Content is created.
        simulateTapSearch("term");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(2, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc2 = getPanelContentViewCore();
        assertNotSame(cvc1, cvc2);

        waitToPreventDoubleTapRecognition();

        // Simulate a new tap and make sure a new Content is created.
        simulateTapSearch("resolution");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(3, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc3 = getPanelContentViewCore();
        assertNotSame(cvc2, cvc3);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        assertEquals(3, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches load correctly.
     */
    @DisabledTest(message = "crbug.com/551711")
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testChainedSearchLoadsCorrectSearchTerm()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        waitToPreventDoubleTapRecognition();

        // Now simulate a long press, leaving the Panel peeking.
        simulateLongPressSearch("resolution");

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();
        assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        ContentViewCore cvc2 = getPanelContentViewCore();
        assertNotSame(cvc1, cvc2);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches make Content visible when opening the Panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testChainedSearchContentVisibility() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        waitToPreventDoubleTapRecognition();

        // Now simulate a long press, leaving the Panel peeking.
        simulateLongPressSearch("resolution");
        assertNeverCalledContentViewCoreOnShow();
        assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();
        assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        ContentViewCore cvc2 = getPanelContentViewCore();
        assertNotSame(cvc1, cvc2);
    }

    //============================================================================================
    // History Removal Tests
    //============================================================================================

    /**
     * Tests that a tap followed by closing the Panel removes the loaded URL from history.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testTapCloseRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Close the Panel without seeing the Content.
        tapBasePageToClosePanel();

        // Now check that the URL has been removed from history.
        assertTrue(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that a tap followed by opening the Panel does not remove the loaded URL from history.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapExpandNotRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Expand Panel so that the Content becomes visible.
        tapPeekingBarToExpandAndAssert();

        // Close the Panel.
        tapBasePageToClosePanel();

        // Now check that the URL has not been removed from history, since the Content was seen.
        assertFalse(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that chained searches without opening the Panel removes all loaded URLs from history.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testChainedTapsRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        String url1 = mFakeServer.getLoadedUrl();
        assertNotNull(url1);

        waitToPreventDoubleTapRecognition();

        // Simulate another tap and make sure another URL was loaded.
        simulateTapSearch("term");
        String url2 = mFakeServer.getLoadedUrl();
        assertNotSame(url1, url2);

        waitToPreventDoubleTapRecognition();

        // Simulate another tap and make sure another URL was loaded.
        simulateTapSearch("resolution");
        String url3 = mFakeServer.getLoadedUrl();
        assertNotSame(url2, url3);

        // Close the Panel without seeing any Content.
        tapBasePageToClosePanel();

        // Now check that all three URLs have been removed from history.
        assertEquals(3, mFakeServer.getLoadedUrlCount());
        assertTrue(mFakeServer.hasRemovedUrl(url1));
        assertTrue(mFakeServer.hasRemovedUrl(url2));
        assertTrue(mFakeServer.hasRemovedUrl(url3));
    }

    //============================================================================================
    // Translate Tests
    //============================================================================================

    /**
     * Tests that a simple Tap with language determination triggers translation.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true")
    @RetryOnFailure
    public void testTapWithLanguage() throws InterruptedException, TimeoutException {
        // Tapping a German word should trigger translation.
        simulateTapSearch("german");

        // Make sure we tried to trigger translate.
        assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a simple Tap without language determination does not trigger translation.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true")
    public void testTapWithoutLanguage() throws InterruptedException, TimeoutException {
        // Tapping an English word should NOT trigger translation.
        simulateTapSearch("search");

        // Make sure we did not try to trigger translate.
        assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that the server-controlled-onebox flag can override behavior on a simple Tap
     * without language determination.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add({ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true",
            ContextualSearchFieldTrial.ENABLE_SERVER_CONTROLLED_ONEBOX + "=true"})
    @RetryOnFailure
    public void testTapWithoutLanguageCanBeForced() throws InterruptedException, TimeoutException {
        // Tapping an English word should trigger translation.
        simulateTapSearch("search");

        // Make sure we did try to trigger translate.
        assertTrue(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a long-press does trigger translation.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true")
    @RetryOnFailure
    public void testLongpressTranslates() throws InterruptedException, TimeoutException {
        // LongPress on any word should trigger translation.
        simulateLongPressSearch("search");

        // Make sure we did try to trigger translate.
        assertTrue(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a long-press does NOT trigger translation when auto-detect is disabled.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add({ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true",
            ContextualSearchFieldTrial.DISABLE_AUTO_DETECT_TRANSLATION_ONEBOX + "=true"})
    @RetryOnFailure
    public void testLongpressAutoDetectDisabledDoesNotTranslate()
            throws InterruptedException, TimeoutException {
        // Unless disabled, LongPress on any word should trigger translation.
        simulateLongPressSearch("search");

        // Make sure we did not try to trigger translate.
        assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a long-press does NOT trigger translation when general one-box is disabled.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add({ContextualSearchFieldTrial.ENABLE_TRANSLATION + "=true",
            ContextualSearchFieldTrial.DISABLE_FORCE_TRANSLATION_ONEBOX + "=true"})
    @RetryOnFailure
    public void testLongpressTranslateDisabledDoesNotTranslate()
            throws InterruptedException, TimeoutException {
        // Unless disabled, LongPress on any word should trigger translation.
        simulateLongPressSearch("search");

        // Make sure we did not try to trigger translate.
        assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that Contextual Search works in fullscreen. Specifically, tests that tapping a word
     * peeks the panel, expanding the bar results in the bar ending at the correct spot in the page
     * and tapping the base page closes the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapContentAndExpandPanelInFullscreen()
            throws InterruptedException, TimeoutException {
        // Toggle tab to fulllscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(getActivity().getActivityTab(),
                true, getActivity());

        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Expand the panel and assert that it ends up in the right place.
        tapPeekingBarToExpandAndAssert();
        assertEquals(mManager.getContextualSearchPanel().getHeight(),
                mManager.getContextualSearchPanel().getPanelHeightFromState(PanelState.EXPANDED));

        // Tap the base page and assert that the panel is closed.
        tapBasePageToClosePanel();
    }

    /**
     * Tests that the Contextual Search panel is dismissed when entering or exiting fullscreen.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testPanelDismissedOnToggleFullscreen()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Toggle tab to fullscreen.
        Tab tab = getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, true, getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();

        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Toggle tab to non-fullscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, false, getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();
    }

    /**
     * Tests that ContextualSearchImageControl correctly sets either the icon sprite or thumbnail
     * as visible.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testImageControl() throws InterruptedException, TimeoutException {
        simulateTapSearch("search");

        final ContextualSearchImageControl imageControl = mPanel.getImageControl();
        final ContextualSearchIconSpriteControl iconSpriteControl =
                imageControl.getIconSpriteControl();

        assertTrue(iconSpriteControl.isVisible());
        assertFalse(imageControl.getThumbnailVisible());
        assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                imageControl.setThumbnailUrl("http://someimageurl.com/image.png");
                imageControl.onThumbnailFetched(true);
            }
        });

        assertTrue(imageControl.getThumbnailVisible());
        assertEquals(imageControl.getThumbnailUrl(), "http://someimageurl.com/image.png");

        // The switch between the icon sprite and thumbnail is animated. Poll the UI thread to
        // check that the icon sprite is hidden at the end of the animation.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !iconSpriteControl.isVisible();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                imageControl.hideStaticImage(false);
            }
        });

        assertTrue(iconSpriteControl.isVisible());
        assertFalse(imageControl.getThumbnailVisible());
        assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));
    }

    // TODO(twellington): Add an end-to-end integration test for fetching a thumbnail based on a
    //                    a URL that is included with the resolution response.

    /**
     * Tests that Contextual Search is fully disabled when offline.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    // NOTE: Remove the flag so we will run just this test with onLine detection enabled.
    @CommandLineFlags.Remove(ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED)
    public void testNetworkDisconnectedDeactivatesSearch()
            throws InterruptedException, TimeoutException {
        setOnlineStatusAndReload(false);
        longPressNodeWithoutWaiting("states");
        waitForSelectActionBarVisible();
        // Verify the panel didn't open.  It should open by now if CS has not been disabled.
        // TODO(donnd): Consider waiting for some condition to be sure we'll catch all failures,
        // e.g. in case the Bar is about to show but has not yet appeared.  Currently catches ~90%.
        assertPanelClosedOrUndefined();

        // Similar sequence with network connected should peek for Longpress.
        setOnlineStatusAndReload(true);
        longPressNodeWithoutWaiting("states");
        waitForSelectActionBarVisible();
        waitForPanelToPeek();
    }

    /**
     * Tests that the quick action caption is set correctly when one is available. Also tests that
     * the caption gets changed when the panel is expanded and reset when the panel is closed.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionCaptionAndImage() throws InterruptedException, TimeoutException {
        // Simulate a tap to show the Bar, then set the quick action data.
        simulateTapSearch("search");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                        QuickActionCategory.PHONE);
                // Finish all running animations.
                mPanel.onUpdateAnimation(System.currentTimeMillis(), true);
            }
        });

        ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
        ContextualSearchQuickActionControl quickActionControl = barControl.getQuickActionControl();
        ContextualSearchImageControl imageControl = mPanel.getImageControl();
        final ContextualSearchIconSpriteControl iconSpriteControl =
                imageControl.getIconSpriteControl();

        // Check that the peeking bar is showing the quick action data.
        assertTrue(quickActionControl.hasQuickAction());
        assertTrue(barControl.getCaptionVisible());
        assertEquals(getActivity().getResources().getString(
                R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        assertEquals(1.f, imageControl.getStaticImageVisibilityPercentage());

        // Expand the bar.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.simulateTapOnEndButton();
            }
        });
        waitForPanelToExpand();

        // Check that the expanded bar is showing the correct image and caption.
        assertTrue(barControl.getCaptionVisible());
        assertEquals(getActivity().getResources().getString(
                ContextualSearchCaptionControl.EXPANED_CAPTION_ID),
                barControl.getCaptionText());
        assertEquals(0.f, imageControl.getStaticImageVisibilityPercentage());
        assertTrue(iconSpriteControl.isVisible());

        // Go back to peeking.
        swipePanelDown();
        waitForPanelToPeek();

        // Assert that the quick action data is showing.
        assertTrue(barControl.getCaptionVisible());
        assertEquals(getActivity().getResources().getString(
                R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        assertEquals(1.f, imageControl.getStaticImageVisibilityPercentage());
    }

    /**
     * Tests that an intent is sent when the bar is tapped and a quick action is available.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionIntent() throws InterruptedException, TimeoutException {
        // Add a new filter to the activity monitor that matches the intent that should be fired.
        IntentFilter quickActionFilter = new IntentFilter(Intent.ACTION_VIEW);
        quickActionFilter.addDataScheme("tel");
        mActivityMonitor = getInstrumentation().addMonitor(
                quickActionFilter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                true);

        // Simulate a tap to show the Bar, then set the quick action data.
        simulateTapSearch("search");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                        QuickActionCategory.PHONE);
            }
        });

        // Tap on the portion of the bar that should trigger the quick action intent to be fired.
        clickPanelBar(0.95f);

        // Assert that an intent was fired.
        assertEquals(1, mActivityMonitor.getHits());
    }
}
