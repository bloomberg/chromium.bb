// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForTabs;
import static org.chromium.content.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Point;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchCaptionControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchQuickActionControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeSlowResolveSearch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.widget.findinpage.FindToolbar;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestSelectionPopupController;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

import javax.annotation.Nullable;

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
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION,
        "disable-features=" + ChromeFeatureList.FULLSCREEN_ACTIVITY})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@RetryOnFailure
public class ContextualSearchManagerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE =
            "/chrome/test/data/android/contextualsearch/tap_test.html";
    private static final int TEST_TIMEOUT = 15000;
    private static final int TEST_EXPECTED_FAILURE_TIMEOUT = 1000;
    private static final int PLENTY_OF_TAPS = 1000;

    // TODO(donnd): get these from TemplateURL once the low-priority or Contextual Search API
    // is fully supported.
    private static final String NORMAL_PRIORITY_SEARCH_ENDPOINT = "/search?";
    private static final String LOW_PRIORITY_SEARCH_ENDPOINT = "/s?";
    private static final String LOW_PRIORITY_INVALID_SEARCH_ENDPOINT = "/s/invalid";
    private static final String CONTEXTUAL_SEARCH_PREFETCH_PARAM = "&pf=c";
    // The number of ms to delay startup for all tests.
    private static final int ACTIVITY_STARTUP_DELAY_MS = 1000;

    // Ranker data that's expected to be logged.
    private static final Set<ContextualSearchRankerLogger.Feature> EXPECTED_RANKER_OUTCOMES;
    static {
        Set<ContextualSearchRankerLogger.Feature> expectedOutcomes =
                new HashSet<ContextualSearchRankerLogger.Feature>(
                        ContextualSearchRankerLoggerImpl.OUTCOMES.keySet());
        // We don't log whether the quick action was clicked unless we actually have a quick action.
        expectedOutcomes.remove(
                ContextualSearchRankerLogger.Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED);
        EXPECTED_RANKER_OUTCOMES = Collections.unmodifiableSet(expectedOutcomes);
    }
    private static final Set<ContextualSearchRankerLogger.Feature> EXPECTED_RANKER_FEATURES;
    static {
        Set<ContextualSearchRankerLogger.Feature> expectedFeatures =
                new HashSet<ContextualSearchRankerLogger.Feature>(
                        ContextualSearchRankerLoggerImpl.FEATURES.keySet());
        // We don't log previous user impressions and CTR if not available for the current user.
        expectedFeatures.remove(ContextualSearchRankerLogger.Feature.PREVIOUS_WEEK_CTR_PERCENT);
        expectedFeatures.remove(
                ContextualSearchRankerLogger.Feature.PREVIOUS_WEEK_IMPRESSIONS_COUNT);
        expectedFeatures.remove(ContextualSearchRankerLogger.Feature.PREVIOUS_28DAY_CTR_PERCENT);
        expectedFeatures.remove(
                ContextualSearchRankerLogger.Feature.PREVIOUS_28DAY_IMPRESSIONS_COUNT);
        EXPECTED_RANKER_FEATURES = Collections.unmodifiableSet(expectedFeatures);
    }

    private ActivityMonitor mActivityMonitor;
    private ContextualSearchFakeServer mFakeServer;
    private ContextualSearchManager mManager;
    private ContextualSearchPanel mPanel;
    private ContextualSearchPolicy mPolicy;
    private ContextualSearchSelectionController mSelectionController;
    private EmbeddedTestServer mTestServer;
    private boolean mPollInstrumentationThread;

    private float mDpToPx;

    // State for an individual test.
    FakeSlowResolveSearch mLatestSlowResolveSearch;

    @Before
    public void setUp() throws Exception {
        // We have to set up the test server before starting the activity.
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
        // There's a problem with immediate startup that causes flakes due to the page not being
        // ready, so specify a startup-delay of 1000 for legacy behavior.  See crbug.com/635661.
        // TODO(donnd): find a better way to wait for page-ready, or at least reduce the delay!
        Thread.sleep(ACTIVITY_STARTUP_DELAY_MS);
        mManager = mActivityTestRule.getActivity().getContextualSearchManager();
        mManager.suppressContextualSearchForSmartSelection(false);

        Assert.assertNotNull(mManager);
        mPanel = mManager.getContextualSearchPanel();

        mSelectionController = mManager.getSelectionController();
        mPolicy = mManager.getContextualSearchPolicy();
        mPolicy.overrideDecidedStateForTesting(true);
        resetCounters();

        mFakeServer = new ContextualSearchFakeServer(mPolicy, this, mManager,
                mManager.getOverlayContentDelegate(), new OverlayContentProgressObserver(),
                mActivityTestRule.getActivity());

        mPanel.setOverlayPanelContentFactory(mFakeServer);
        mManager.setNetworkCommunicator(mFakeServer);

        registerFakeSearches();

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        mActivityMonitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);

        mDpToPx = mActivityTestRule.getActivity().getResources().getDisplayMetrics().density;
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(false);
            }
        });
    }

    /**
     * @return The {@link ContextualSearchPanel}.
     */
    ContextualSearchPanel getPanel() {
        return mPanel;
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
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
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
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        DOMUtils.longPressNode(tab.getContentViewCore(), nodeId);
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
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        DOMUtils.clickNode(tab.getContentViewCore(), nodeId);
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     * @param text The string to wait for the selection to become.
     */
    public void waitForSelectionToBe(final String text) {
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
            final ContextualSearchFakeServer.FakeTapSearch search) {
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
            final ContextualSearchFakeServer.FakeTapSearch search) {
        CriteriaHelper.pollInstrumentationThread(new Criteria("Fake Search was never ready.") {
            @Override
            public boolean isSatisfied() {
                return search.didFinishSearchTermResolution();
            }
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for a Normal priority URL to be loaded, or asserts that the load never happened.
     * This is needed when we test with a live internet connection and an invalid url fails to
     * load (as expected.  See crbug.com/682953 for background.
     */
    private void waitForNormalPriorityUrlLoaded() {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Normal priority URL was not loaded: "
                        + String.valueOf(mFakeServer.getLoadedUrl())) {
                    @Override
                    public boolean isSatisfied() {
                        return mFakeServer.getLoadedUrl() != null
                                && mFakeServer.getLoadedUrl().contains(
                                           NORMAL_PRIORITY_SEARCH_ENDPOINT);
                    }
                },
                TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Runs the given Runnable in the main thread.
     * @param runnable The Runnable.
     */
    public void runOnMainSync(Runnable runnable) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(runnable);
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
            InstrumentationRegistry.getInstrumentation().runOnMainSync(new FakeResponseOnMainThread(
                    isNetworkUnavailable, responseCode, searchTerm, displayText, alternateTerm, mid,
                    doPreventPreload, startAdjust, endAdjust, contextLanguage, thumbnailUrl,
                    caption, quickActionUri, quickActionCategory));
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
        return mFakeServer.isContentVisible();
    }

    /**
     * Asserts that the Panel's ContentViewCore is created.
     */
    private void assertContentViewCoreCreated() {
        Assert.assertNotNull(getPanelContentViewCore());
    }

    /**
     * Asserts that the Panel's ContentViewCore is not created.
     */
    private void assertNoContentViewCore() {
        Assert.assertNull(getPanelContentViewCore());
    }

    /**
     * Asserts that the Panel's ContentViewCore is visible.
     */
    private void assertContentViewCoreVisible() {
        Assert.assertTrue(isContentViewCoreVisible());
    }

    /**
     * Asserts that the Panel's WebContents.onShow() method was never called.
     */
    private void assertNeverCalledWebContentsOnShow() {
        Assert.assertFalse(mFakeServer.didEverCallWebContentsOnShow());
    }

    /**
     * Asserts that the Panel's ContentViewCore is created
     */
    private void assertContentViewCoreCreatedButNeverMadeVisible() {
        assertContentViewCoreCreated();
        Assert.assertFalse(isContentViewCoreVisible());
        assertNeverCalledWebContentsOnShow();
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
     * A SelectionPopupController that has some methods stubbed out for testing.
     */
    private static final class StubbedSelectionPopupController
            extends TestSelectionPopupController {
        private boolean mIsFocusedNodeEditable;

        public StubbedSelectionPopupController() {}

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
        KeyUtils.singleKeyEventActivity(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), keycode);
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
        Assert.assertTrue(
                "Expected to find searchTerm '" + searchTerm + "', " + message, doesMatch);
    }

    /**
     * Asserts that the given parameters are present in the most recently loaded URL.
     */
    private void assertContainsParameters(String searchTerm, String alternateTerm) {
        Assert.assertTrue(mFakeServer.getSearchTermRequested() == null
                || (mFakeServer.getLoadedUrl().contains(searchTerm)
                           && mFakeServer.getLoadedUrl().contains(alternateTerm)));
    }

    /**
     * Asserts that a Search Term has been requested.
     */
    private void assertSearchTermRequested() {
        Assert.assertNotNull(mFakeServer.getSearchTermRequested());
    }

    /**
     * Asserts that there has not been any Search Term requested.
     */
    private void assertSearchTermNotRequested() {
        Assert.assertNull(mFakeServer.getSearchTermRequested());
    }

    /**
     * Asserts that the panel is currently closed or in an undefined state.
     */
    private void assertPanelClosedOrUndefined() {
        boolean success = false;
        if (mPanel == null) {
            success = true;
        } else {
            PanelState panelState = mPanel.getPanelState();
            success = panelState == PanelState.CLOSED || panelState == PanelState.UNDEFINED;
        }
        Assert.assertTrue(success);
    }

    /**
     * Asserts that no URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedNoUrl() {
        Assert.assertTrue("Requested a search or preload when none was expected!",
                mFakeServer.getLoadedUrl() == null);
    }

    /**
     * Asserts that a low-priority URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedLowPriorityUrl() {
        String message = "Expected a low priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(LOW_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that a low-priority URL that is intentionally invalid has been loaded in the Overlay
     * Panel (in order to produce an error).
     */
    private void assertLoadedLowPriorityInvalidUrl() {
        String message = "Expected a low priority invalid search request URL, but got "
                + (String.valueOf(mFakeServer.getLoadedUrl()));
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(
                                   LOW_PRIORITY_INVALID_SEARCH_ENDPOINT));
        Assert.assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that a normal priority URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedNormalPriorityUrl() {
        String message = "Expected a normal priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue(
                "Normal priority request should not have the prefetch parameter, but did!",
                mFakeServer.getLoadedUrl() != null
                        && !mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that no URLs have been loaded in the Overlay Panel since the last
     * {@link ContextualSearchFakeServer#reset}.
     */
    private void assertNoSearchesLoaded() {
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        assertLoadedNoUrl();
    }

    /**
     * Asserts that the tap triggered promo counter is enabled and at the specified count.
     */
    private void assertTapPromoCounterEnabledAt(int expectedCount) {
        Assert.assertTrue(mPolicy.getPromoTapCounter().isEnabled());
        Assert.assertEquals(expectedCount, mPolicy.getPromoTapCounter().getCount());
    }

    /**
     * Asserts that the tap triggered promo counter is disabled and at the specified count.
     */
    private void assertTapPromoCounterDisabledAt(int expectedCount) {
        Assert.assertFalse(mPolicy.getPromoTapCounter().isEnabled());
        Assert.assertEquals(expectedCount, mPolicy.getPromoTapCounter().getCount());
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
     * Waits on the UI thread unless mPollInstrumentationThread is set.
     * @param state The {@link PanelState} to wait for.
     */
    private void waitForPanelToEnterState(final PanelState state) {
        final Criteria panelStateCriteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (mPanel == null) return false;
                updateFailureReason("Panel did not enter " + state + " state. "
                        + "Instead, the current state is " + mPanel.getPanelState() + ".");
                return mPanel.getPanelState() == state && !mPanel.isHeightAnimationRunning();
            }
        };
        if (mPollInstrumentationThread) {
            CriteriaHelper.pollInstrumentationThread(panelStateCriteria);
        } else {
            CriteriaHelper.pollUiThread(panelStateCriteria);
        }
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
        Assert.assertFalse(didChangeState);
    }

    /**
     * Shorthand for a common sequence:
     * 1) Waits for gesture processing,
     * 2) Waits for the panel to close,
     * 3) Asserts that there is no selection and that the panel closed.
     * @throws InterruptedException
     */
    private void waitForGestureToClosePanelAndAssertNoSelection() throws InterruptedException {
        waitForPanelToClose();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
    }

    /**
     * Waits for the selection to be empty.
     * Waits on the UI thread unless mPollInstrumentationThread is set.
     * Use this method any time a test repeatedly establishes and dissolves a selection to ensure
     * that the selection has been completely dissolved before simulating the next selection event.
     * This is needed because the renderer's notification of a selection going away is async,
     * and a subsequent tap may think there's a current selection until it has been dissolved.
     */
    private void waitForSelectionEmpty() {
        final Criteria selectionEmptyCriteria = new Criteria("Selection never empty.") {
            @Override
            public boolean isSatisfied() {
                return mSelectionController.isSelectionEmpty();
            }
        };
        if (mPollInstrumentationThread) {
            CriteriaHelper.pollInstrumentationThread(selectionEmptyCriteria);
        } else {
            CriteriaHelper.pollUiThread(selectionEmptyCriteria);
        }
    }

    /**
     * Waits for the panel to close and then waits for the selection to dissolve.
     */
    private void waitForPanelToCloseAndSelectionEmpty() throws InterruptedException {
        waitForPanelToClose();
        waitForSelectionEmpty();
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
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(mActivityTestRule.getActivity(), dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(mActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, stepCount, downTime);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
    }

    /**
     * Generate a swipe sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void swipe(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        int halfCount = stepCount / 2;
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(mActivityTestRule.getActivity(), dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(mActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, halfCount, downTime);
        // Generate events in the stationary end position in order to simulate a "pause" in
        // the movement, therefore preventing this gesture from being interpreted as a fling.
        TouchCommon.dragTo(mActivityTestRule.getActivity(), dragEndX, dragEndX, dragEndY, dragEndY,
                halfCount, downTime);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
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
        // This taps on the panel in an area that will be selected if the "intelligence" node has
        // been tap-selected, and that will cause it to be long-press selected.
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
        View root = mActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
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
        waitForSelectionEmpty();
    }

    /**
     * Tap on the peeking Bar to expand the panel, then taps on the base page to close it.
     */
    private void tapBarToExpandAndClosePanel() throws InterruptedException {
        tapPeekingBarToExpandAndAssert();
        tapBasePageToClosePanel();
    }

    /**
     * Generate a click in the middle of panel's bar.
     * TODO(donnd): Replace this method with panelBarClick since this appears to be unreliable.
     */
    private void clickPanelBar() {
        View root = mActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
        float tapX = ((mPanel.getOffsetX() + mPanel.getWidth()) / 2f) * mDpToPx;
        float tapY = (mPanel.getOffsetY() + (mPanel.getBarContainerHeight() / 2f)) * mDpToPx;

        TouchCommon.singleClickView(root, (int) tapX, (int) tapY);
    }

    /**
     * Taps the peeking bar to expand the panel
     */
    private void tapPeekingBarToExpandAndAssert() throws InterruptedException {
        clickPanelBar();
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
        closePanel();
        waitForPanelToCloseAndSelectionEmpty();
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
                SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
                boolean freStatus = prefs.getBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, false);
                prefs.edit()
                        .clear()
                        .putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, freStatus)
                        .apply();
            }
        });
    }

    /**
     * Force the Panel to handle a click in the Bar.
     * @throws InterruptedException
     */
    private void forcePanelToHandleBarClick() throws InterruptedException {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // TODO(donnd): provide better time and x,y data to make this more broadly useful.
                mPanel.handleBarClick(0, 0);
            }
        });
    }

    /**
     * Force the Panel to close.
     * @throws InterruptedException
     */
    private void closePanel() throws InterruptedException {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
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

    /** Gets the Ranker Logger and asserts if we can't. **/
    private ContextualSearchRankerLoggerImpl getRankerLogger() {
        ContextualSearchRankerLoggerImpl rankerLogger =
                (ContextualSearchRankerLoggerImpl) mManager.getRankerLogger();
        Assert.assertNotNull(rankerLogger);
        return rankerLogger;
    }

    /** @return The value of the given logged feature, or {@code null} if not logged. */
    private Object loggedToRanker(ContextualSearchRankerLogger.Feature feature) {
        return getRankerLogger().getFeaturesLogged().get(feature);
    }

    /** Asserts that all the expected features have been logged to Ranker. **/
    private void assertLoggedAllExpectedFeaturesToRanker() {
        for (ContextualSearchRankerLogger.Feature feature : EXPECTED_RANKER_FEATURES) {
            Assert.assertNotNull(loggedToRanker(feature));
        }
    }

    /** Asserts that all the expected outcomes have been logged to Ranker. **/
    private void assertLoggedAllExpectedOutcomesToRanker() {
        for (ContextualSearchRankerLogger.Feature feature : EXPECTED_RANKER_OUTCOMES) {
            Assert.assertNotNull("Expected this outcome to be logged: " + feature,
                    getRankerLogger().getOutcomesLogged().get(feature));
        }
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests whether the contextual search panel hides when omnibox is clicked.
     */
    //@SmallTest
    //@Feature({"ContextualSearch"})
    @Test
    @DisabledTest
    public void testHidesWhenOmniboxFocused() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        Assert.assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();

        OmniboxTestUtils.toggleUrlBarFocus(
                (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar), true);

        assertPanelClosedOrUndefined();
    }

    /**
     * Tests the doesContainAWord method.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoesContainAWord() {
        Assert.assertTrue(mSelectionController.doesContainAWord("word"));
        Assert.assertTrue(mSelectionController.doesContainAWord("word "));
        Assert.assertFalse("Emtpy string should not be considered a word!",
                mSelectionController.doesContainAWord(""));
        Assert.assertFalse("Special symbols should not be considered a word!",
                mSelectionController.doesContainAWord("@"));
        Assert.assertFalse("White space should not be considered a word",
                mSelectionController.doesContainAWord(" "));
        Assert.assertTrue(mSelectionController.doesContainAWord("Q2"));
        Assert.assertTrue(mSelectionController.doesContainAWord("123"));
    }

    /**
     * Tests the isValidSelection method.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsValidSelection() {
        StubbedSelectionPopupController c = new StubbedSelectionPopupController();
        Assert.assertTrue(mSelectionController.isValidSelection("valid", c));
        Assert.assertFalse(mSelectionController.isValidSelection(" ", c));
        c.setIsFocusedNodeEditableForTest(true);
        Assert.assertFalse(mSelectionController.isValidSelection("editable", c));
        c.setIsFocusedNodeEditableForTest(false);
        String numberString = "0123456789";
        StringBuilder longStringBuilder = new StringBuilder();
        for (int i = 0; i < 11; i++) longStringBuilder.append(numberString);
        Assert.assertTrue(mSelectionController.isValidSelection(numberString, c));
        Assert.assertFalse(mSelectionController.isValidSelection(longStringBuilder.toString(), c));
    }

    /**
     * Tests a simple Tap.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTap() throws InterruptedException, TimeoutException {
        simulateTapSearch("intelligence");
        assertLoadedLowPriorityUrl();

        assertLoggedAllExpectedFeaturesToRanker();
        Assert.assertEquals(
                true, loggedToRanker(ContextualSearchRankerLogger.Feature.IS_LONG_WORD));
        // The panel must be closed for outcomes to be logged.
        // Close the panel by clicking far away in order to make sure the outcomes get logged by
        // the hideContextualSearchUi call to writeRankerLoggerOutcomesAndReset.
        clickWordNode("states-far");
        waitForPanelToClose();
        assertLoggedAllExpectedOutcomesToRanker();
    }

    /**
     * Tests a simple Long-Press gesture, without opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongPress() throws InterruptedException, TimeoutException {
        longPressNode("states");

        Assert.assertNull(mFakeServer.getSearchTermRequested());
        waitForPanelToPeek();
        assertLoadedNoUrl();
        assertNoContentViewCore();
    }

    /**
     * Tests swiping the overlay open, after an initial tap that activates the peeking card.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
    public void testSwipeExpand() throws InterruptedException, TimeoutException {
        assertNoSearchesLoaded();
        clickWordNode("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeek();
        flingPanelUp();
        waitForPanelToExpand();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial long-press that activates the peeking card,
     * followed by closing the panel.
     * This test also verifies that we don't create any {@link ContentViewCore} or load any URL
     * until the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testLongPressSwipeExpand() throws InterruptedException, TimeoutException {
        simulateLongPressSearch("search");
        assertNoContentViewCore();
        assertLoadedNoUrl();

        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // tap the base page to close.
        closePanel();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertNoContentViewCore();
    }

    /**
     * Tests that only a single low-priority request is issued for a Tap/Open sequence.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCausesOneLowPriorityRequest() throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the bar opens, we should not make any additional request.
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that a failover for a prefetch request is issued after the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPrefetchFailoverRequestMadeAfterOpen()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request fails, we should not automatically issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that a live request that fails (for an invalid URL) does a failover to a
     * normal priority request once the user triggers the failover by opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLivePrefetchFailoverRequestMadeAfterOpen()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        mFakeServer.setLowPriorityPathInvalid();
        simulateTapSearch("search");
        assertLoadedLowPriorityInvalidUrl();
        Assert.assertTrue(mFakeServer.didAttemptLoadInvalidUrl());

        // we should not automatically issue a new request.
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Fake a navigation error if offline.
        // When connected to the Internet this error may already have happened due to actually
        // trying to load the invalid URL.  But on test bots that are not online we need to
        // fake that a navigation happened with an error. See crbug.com/682953 for details.
        if (!mManager.isOnline()) {
            boolean isFailure = true;
            fakeContentViewDidNavigate(isFailure);
        }

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        waitForNormalPriorityUrlLoaded();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests a simple Tap with disable-preload set.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapDisablePreload() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");

        assertSearchTermRequested();
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", true);
        assertLoadedNoUrl();
        waitForPanelToPeek();
        assertLoadedNoUrl();
    }

    /**
     * Tests that long-press selects text, and a subsequent tap will unselect text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongPressGestureSelects() throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedNoUrl();  // No load after long-press until opening panel.
        clickNode("question-mark");
        waitForPanelToCloseAndSelectionEmpty();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture selects the expected text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureSelects() throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertNull(getSelectedText());
    }

    /**
     * Tests that a Tap gesture on a special character does not select or show the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureOnSpecialCharacterDoesntSelect()
            throws InterruptedException, TimeoutException {
        clickNode("question-mark");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture followed by scrolling clears the selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFollowedByScrollClearsSelection()
            throws InterruptedException, TimeoutException {
        clickWordNode("intelligence");
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(mSelectionController.getSelectedText()));
    }

    /**
     * Tests that a Tap gesture followed by tapping an invalid character doesn't select.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFollowedByInvalidTextTapCloses()
            throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping a non-text character doesn't select.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     * crbug.com/665633
     */
    @Test
    @DisabledTest
    public void testTapGestureFollowedByNonTextTap() throws InterruptedException, TimeoutException {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("button");
        waitForPanelToCloseAndSelectionEmpty();
    }

    /**
     * Tests that a Tap gesture far away toggles selecting text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFarAwayTogglesSelecting()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        clickNode("states-far");
        waitForPanelToClose();
        Assert.assertNull(getSelectedText());
        clickNode("states-far");
        waitForPanelToPeek();
        Assert.assertEquals("States", getSelectedText());
    }

    /**
     * Tests a "retap" -- that sequential Tap gestures nearby keep selecting.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGesturesNearbyKeepSelecting() throws InterruptedException, TimeoutException {
        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        assertLoggedAllExpectedFeaturesToRanker();
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
        assertLoggedAllExpectedOutcomesToRanker();
        assertLoggedAllExpectedFeaturesToRanker();
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        clickNode("states");
        waitForSelectionToBe("States");
        assertLoggedAllExpectedOutcomesToRanker();
    }

    /**
     * Tests that a long-press gesture followed by scrolling does not clear the selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongPressGestureFollowedByScrollMaintainsSelection()
            throws InterruptedException, TimeoutException {
        longPressNode("intelligence");
        waitForPanelToPeek();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedNoUrl();
    }

    /**
     * Tests that a long-press gesture followed by a tap does not select.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/818897")
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testContextualSearchDismissedOnForegroundTabCrash()
            throws InterruptedException, TimeoutException {
        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getActivityTab().simulateRendererKilledForTesting(
                        true);
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testContextualSearchNotDismissedOnBackgroundTabCrash()
            throws InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mActivityTestRule.getActivity());
        final Tab tab2 =
                TabModelUtils.getCurrentTab(mActivityTestRule.getActivity().getCurrentTabModel());

        // TODO(donnd): consider using runOnUiThreadBlocking, won't need to waitForIdleSync?
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(mActivityTestRule.getActivity().getCurrentTabModel(), 0);
            }
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
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
        mActivityTestRule.getActivity().getTabModelSelector().addObserver(observer);

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
        mActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA role does not trigger.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnRoleIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("role");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA attribute does not trigger.
     * http://crbug.com/542874
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnARIAIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("aria");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element that is focusable does not trigger.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnFocusableIgnored() throws InterruptedException, TimeoutException {
        PanelState initialState = mPanel.getPanelState();
        clickNode("focusable");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests expanding the panel before the search term has resolved, verifies that nothing
     * loads until the resolve completes and that it's now a normal priority URL.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testHttpsAfterAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);
        mFakeServer.setShouldUseHttps(true);

        clickToResolveAndAssertPrefetch();
    }

    /**
     * Tests that HTTP does resolve in the opt-out model before the user accepts.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testHttpBeforeAcceptForOptOut() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        clickToResolveAndAssertPrefetch();
    }

    /**
     * Tests that HTTP does resolve in the opt-out model after the user accepts.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
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
    private void assertAppMenuVisibility(final boolean isVisible) {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(isVisible, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mActivityTestRule.getActivity()
                                .getAppMenuHandler()
                                .isAppMenuShowing();
                    }
                }));
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is expanded.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
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
    // Promo open count
    // --------------------------------------------------------------------------------------------

    /**
     * Tests the promo open counter.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testPromoOpenCountForUndecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(false);

        // A simple click / resolve / prefetch sequence without open should not change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());

        // An open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(1, mPolicy.getPromoOpenCount());

        // Another open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());

        // Once the user has decided, we should stop counting.
        mPolicy.overrideDecidedStateForTesting(true);
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());
    }

    /**
     * Tests the promo open counter.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testPromoOpenCountForDecided() throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);

        // An open should not count for decided users.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());
    }

    // --------------------------------------------------------------------------------------------
    // Tap count - number of taps between opens.
    // --------------------------------------------------------------------------------------------
    /**
     * Tests the counter for the number of taps between opens.
     */
    @Test
    @DisabledTest(message = "crbug.com/800334")
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCount() throws InterruptedException, TimeoutException {
        resetCounters();
        Assert.assertEquals(0, mPolicy.getTapCount());

        // A simple Tap should change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(1, mPolicy.getTapCount());

        // Another Tap should increase the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(2, mPolicy.getTapCount());

        // An open should reset the counter.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getTapCount());
    }

    // --------------------------------------------------------------------------------------------
    // Calls to ContextualSearchObserver.
    // --------------------------------------------------------------------------------------------
    private static class TestContextualSearchObserver implements ContextualSearchObserver {
        private int mShowCount;
        private int mShowRedactedCount;
        private int mHideCount;
        private int mFirstShownLength;
        private int mLastShownLength;

        @Override
        public void onShowContextualSearch(@Nullable GSAContextDisplaySelection selectionContext) {
            mShowCount++;
            if (selectionContext != null
                    && selectionContext.startOffset < selectionContext.endOffset) {
                mLastShownLength = selectionContext.endOffset - selectionContext.startOffset;
                if (mFirstShownLength == 0) mFirstShownLength = mLastShownLength;
            } else {
                mShowRedactedCount++;
            }
        }

        @Override
        public void onHideContextualSearch() {
            mHideCount++;
        }

        /**
         * @return The count of Hide notifications sent to observers.
         */
        int getHideCount() {
            return mHideCount;
        }

        /**
         * @return The count of Show notifications sent to observers.
         */
        int getShowCount() {
            return mShowCount;
        }

        /**
         * @return The count of Show notifications sent to observers that had the data redacted due
         *         to our policy on privacy.
         */
        int getShowRedactedCount() {
            return mShowRedactedCount;
        }

        /**
         * @return The length of the selection for the first Show notification.
         */
        int getFirstShownLength() {
            return mFirstShownLength;
        }

        /**
         * @return The length of the selection for the last Show notification.
         */
        int getLastShownLength() {
            return mLastShownLength;
        }
    }

    /**
     * Tests that a ContextualSearchObserver gets notified when the user brings up The Contextual
     * Search panel via long press and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterLongPress() throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that a ContextualSearchObserver gets notified without any page context when the user
     * is Undecided and our policy disallows sending surrounding text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterLongPressWithoutSurroundings()
            throws InterruptedException, TimeoutException {
        // Mark the user undecided so we won't allow sending surroundings.
        mPolicy.overrideDecidedStateForTesting(false);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel via tap and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterTap() throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        clickWordNode("states");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
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
                return getSelectionPopupController().isSelectActionBarShowing();
            }
        }));
    }

    private SelectionPopupController getSelectionPopupController() {
        return SelectionPopupController.fromWebContents(
                mActivityTestRule.getActivity().getActivityTab().getWebContents());
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel via long press and then dismisses the panel by tapping copy (hide select action mode).
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNotifyObserversOnClearSelectionAfterTap()
            throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        Assert.assertEquals(0, observer.getHideCount());

        // Dismiss select action mode.
        assertWaitForSelectActionBarVisible(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getSelectionPopupController().destroySelectActionMode();
            }
        });
        assertWaitForSelectActionBarVisible(false);

        waitForPanelToClose();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that the Contextual Search panel does not reappear when a long-press selection is
     * modified after the user has taken an action to explicitly dismiss the panel. Also tests
     * that the panel reappears when a new selection is made.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPreventHandlingCurrentSelectionModification()
            throws InterruptedException, TimeoutException {
        simulateLongPressSearch("search");

        // Dismiss the Contextual Search panel.
        closePanel();
        Assert.assertEquals("Search", getSelectedText());

        // Simulate a selection change event and assert that the panel has not reappeared.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SelectionClient selectionClient = mManager.getContextualSearchSelectionClient();
                selectionClient.onSelectionEvent(
                        SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 333, 450);
                selectionClient.onSelectionEvent(
                        SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 303, 450);
            }
        });
        assertPanelClosedOrUndefined();

        // Select a different word and assert that the panel has appeared.
        simulateLongPressSearch("resolution");
        // The simulateLongPressSearch call will verify that the panel peeks.
    }

    /**
     * Tests a bunch of taps in a row.
     * We've had reliability problems with a sequence of simple taps, due to async dissolving
     * of selection bounds, so this helps prevent a regression with that.
     */
    @Test
    @DisabledTest(message = "crbug.com/828780")
    @LargeTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/818897")
    public void testTapALot() throws InterruptedException, TimeoutException {
        for (int i = 0; i < 50; i++) {
            clickToTriggerPrefetch();
            assertSearchTermRequested();
        }
    }

    /**
     * Tests a bunch of taps in a row, with the variation that we wait on the instrumentation
     * thread instead of the UI thread for some wait sequences.
     */
    @Test
    @DisabledTest(message = "crbug.com/828780")
    @LargeTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/818897")
    public void testTapALotInstrumentation() throws InterruptedException, TimeoutException {
        mPollInstrumentationThread = true;
        testTapALot();
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation has a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(mActivityTestRule.getActivity().getActivityTab());
        final NavigationParams navigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, true /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                null /* suggestedFilename */, false /* hasUserGestureCarryover */);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationParams));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an initial
     * navigation has a user gesture but the redirected external navigation doesn't.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRedirectedExternalNavigationWithUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(mActivityTestRule.getActivity().getActivityTab());

        final NavigationParams initialNavigationParams = new NavigationParams("http://test.com", "",
                false /* isPost */, true /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, false /* isExternalProtocol */, true /* isMainFrame */,
                null /* suggestedFilename */, false /* hasUserGestureCarryover */);
        final NavigationParams redirectedNavigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, false /* hasUserGesture */, PageTransition.LINK,
                true /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                null /* suggestedFilename */, false /* hasUserGestureCarryover */);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                OverlayContentDelegate delegate = mManager.getOverlayContentDelegate();
                Assert.assertTrue(delegate.shouldInterceptNavigation(
                        externalNavHandler, initialNavigationParams));
                Assert.assertFalse(delegate.shouldInterceptNavigation(
                        externalNavHandler, redirectedNavigationParams));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation doesn't have a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testExternalNavigationWithoutUserGesture() {
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(mActivityTestRule.getActivity().getActivityTab());
        final NavigationParams navigationParams = new NavigationParams(
                "intent://test/#Intent;scheme=test;package=com.chrome.test;end", "",
                false /* isPost */, false /* hasUserGesture */, PageTransition.LINK,
                false /* isRedirect */, true /* isExternalProtocol */, true /* isMainFrame */,
                null /* suggestedFilename */, false /* hasUserGestureCarryover */);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationParams));
            }
        });
        Assert.assertEquals(0, mActivityMonitor.getHits());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSelectionExpansionOnSearchTermResolution()
            throws InterruptedException, TimeoutException {
        mFakeServer.reset();
        clickWordNode("intelligence");
        waitForPanelToPeek();

        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                null, false, -14, 0, "", "", "", "", QuickActionCategory.NONE);
        waitForSelectionToBe("United States Intelligence");
    }

    //============================================================================================
    // Content Tests
    //============================================================================================

    /**
     * Tests that tap followed by expand makes Content visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapMultipleSwipeOnlyLoadsContentOnce()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests swiping panel up and down after a long press search will only load the Content once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
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
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained tap searches create new Content.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testChainedSearchCreatesNewContent() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        waitToPreventDoubleTapRecognition();

        // Simulate a new tap and make sure a new Content is created.
        simulateTapSearch("term");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc2 = getPanelContentViewCore();
        Assert.assertNotSame(cvc1, cvc2);

        waitToPreventDoubleTapRecognition();

        // Simulate a new tap and make sure a new Content is created.
        simulateTapSearch("resolution");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc3 = getPanelContentViewCore();
        Assert.assertNotSame(cvc2, cvc3);

        // Closing the Panel should destroy the Content.
        closePanel();
        assertNoContentViewCore();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches load correctly.
     */
    @Test
    @DisabledTest(message = "crbug.com/551711")
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testChainedSearchLoadsCorrectSearchTerm()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertContentViewCoreVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        waitToPreventDoubleTapRecognition();

        // Now simulate a long press, leaving the Panel peeking.
        simulateLongPressSearch("resolution");

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        ContentViewCore cvc2 = getPanelContentViewCore();
        Assert.assertNotSame(cvc1, cvc2);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoContentViewCore();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches make Content visible when opening the Panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testChainedSearchContentVisibility() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure Content is not visible.
        simulateTapSearch("search");
        assertContentViewCoreCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        ContentViewCore cvc1 = getPanelContentViewCore();

        waitToPreventDoubleTapRecognition();

        // Now simulate a long press, leaving the Panel peeking.
        simulateLongPressSearch("resolution");
        assertNeverCalledWebContentsOnShow();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertContentViewCoreCreated();
        assertContentViewCoreVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        ContentViewCore cvc2 = getPanelContentViewCore();
        Assert.assertNotSame(cvc1, cvc2);
    }

    //============================================================================================
    // History Removal Tests
    //============================================================================================

    /**
     * Tests that a tap followed by closing the Panel removes the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCloseRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Close the Panel without seeing the Content.
        tapBasePageToClosePanel();

        // Now check that the URL has been removed from history.
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that a tap followed by opening the Panel does not remove the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapExpandNotRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Expand Panel so that the Content becomes visible.
        tapPeekingBarToExpandAndAssert();

        // Close the Panel.
        tapBasePageToClosePanel();

        // Now check that the URL has not been removed from history, since the Content was seen.
        Assert.assertFalse(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that chained searches without opening the Panel removes all loaded URLs from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testChainedTapsRemovedFromHistory() throws InterruptedException, TimeoutException {
        // Simulate a tap and make sure a URL was loaded.
        simulateTapSearch("search");
        String url1 = mFakeServer.getLoadedUrl();
        Assert.assertNotNull(url1);

        waitToPreventDoubleTapRecognition();

        // Simulate another tap and make sure another URL was loaded.
        simulateTapSearch("term");
        String url2 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url1, url2);

        waitToPreventDoubleTapRecognition();

        // Simulate another tap and make sure another URL was loaded.
        simulateTapSearch("resolution");
        String url3 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url2, url3);

        // Close the Panel without seeing any Content.
        closePanel();

        // Now check that all three URLs have been removed from history.
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url1));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url2));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url3));
    }

    //============================================================================================
    // Translate Tests
    //============================================================================================

    /**
     * Tests that a simple Tap with language determination triggers translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapWithLanguage() throws InterruptedException, TimeoutException {
        // Tapping a German word should trigger translation.
        simulateTapSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests translation with a simple Tap can be disabled.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.DISABLE_TRANSLATION + "=true")
    public void testTapDisabled() throws InterruptedException, TimeoutException {
        // Tapping a German word would normally trigger translation, but not with the above flag.
        simulateTapSearch("german");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a simple Tap without language determination does not trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapWithoutLanguage() throws InterruptedException, TimeoutException {
        // Tapping an English word should NOT trigger translation.
        simulateTapSearch("search");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a long-press does trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongpressTranslates() throws InterruptedException, TimeoutException {
        // LongPress on any word should trigger translation.
        simulateLongPressSearch("search");

        // Make sure we did try to trigger translate.
        Assert.assertTrue(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a long-press does NOT trigger translation when disabled.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ContextualSearchFieldTrial.DISABLE_TRANSLATION + "=true")
    public void testLongpressTranslateDisabledDoesNotTranslate()
            throws InterruptedException, TimeoutException {
        // When disabled, LongPress on any word should not trigger translation.
        simulateLongPressSearch("search");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests the Translate Caption on a tap gesture.
     * This test is disabled because it relies on the network and a live search result,
     * which would be flaky for bots.
     */
    @DisabledTest(message = "Useful for manual testing when a network is connected.")
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    public void testTranslateCaption() throws InterruptedException, TimeoutException {
        // Tapping a German word should trigger translation.
        simulateTapSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());

        // Wait for the translate caption to be shown in the Bar.
        int waitFactor = 5; // We need to wait an extra long time for the panel content to render.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
                return barControl != null && barControl.getCaptionVisible()
                        && !TextUtils.isEmpty(barControl.getCaptionText());
            }
        }, 3000 * waitFactor, DEFAULT_POLLING_INTERVAL * waitFactor);
    }

    /**
     * Tests that Contextual Search works in fullscreen. Specifically, tests that tapping a word
     * peeks the panel, expanding the bar results in the bar ending at the correct spot in the page
     * and tapping the base page closes the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTapContentAndExpandPanelInFullscreen()
            throws InterruptedException, TimeoutException {
        // Toggle tab to fulllscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                mActivityTestRule.getActivity().getActivityTab(), true,
                mActivityTestRule.getActivity());

        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Expand the panel and assert that it ends up in the right place.
        tapPeekingBarToExpandAndAssert();
        Assert.assertEquals(mManager.getContextualSearchPanel().getHeight(),
                mManager.getContextualSearchPanel().getPanelHeightFromState(PanelState.EXPANDED),
                0);

        // Tap the base page and assert that the panel is closed.
        tapBasePageToClosePanel();
    }

    /**
     * Tests that the Contextual Search panel is dismissed when entering or exiting fullscreen.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Device(type = {UiDisableIf.PHONE}) // Flaking on phones crbug.com/765796
    public void testPanelDismissedOnToggleFullscreen()
            throws InterruptedException, TimeoutException {
        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Toggle tab to fullscreen.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();

        // Simulate a tap and assert that the panel peeks.
        simulateTapSearch("search");

        // Toggle tab to non-fullscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, mActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();
    }

    /**
     * Tests that ContextualSearchImageControl correctly sets either the icon sprite or thumbnail
     * as visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testImageControl() throws InterruptedException, TimeoutException {
        simulateTapSearch("search");

        final ContextualSearchImageControl imageControl = mPanel.getImageControl();

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                imageControl.setThumbnailUrl("http://someimageurl.com/image.png");
                imageControl.onThumbnailFetched(true);
            }
        });

        Assert.assertTrue(imageControl.getThumbnailVisible());
        Assert.assertEquals(imageControl.getThumbnailUrl(), "http://someimageurl.com/image.png");

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                imageControl.hideCustomImage(false);
            }
        });

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));
    }

    // TODO(twellington): Add an end-to-end integration test for fetching a thumbnail based on a
    //                    a URL that is included with the resolution response.

    /**
     * Tests that Contextual Search is fully disabled when offline.
     */
    @Test
    @DisabledTest(message = "https://crbug.com/761946")
    // @SmallTest
    // @Feature({"ContextualSearch"})
    // // NOTE: Remove the flag so we will run just this test with onLine detection enabled.
    // @CommandLineFlags.Remove(ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED)
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
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionCaptionAndImage() throws InterruptedException, TimeoutException {
        mPanel.getAnimationHandler().enableTestingMode();

        // Simulate a tap to show the Bar, then set the quick action data.
        simulateTapSearch("search");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                        QuickActionCategory.PHONE);
            }
        });

        ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
        ContextualSearchQuickActionControl quickActionControl = barControl.getQuickActionControl();
        ContextualSearchImageControl imageControl = mPanel.getImageControl();

        // Check that the peeking bar is showing the quick action data.
        Assert.assertTrue(quickActionControl.hasQuickAction());
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(mActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Expand the bar.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.simulateTapOnEndButton();
            }
        });
        waitForPanelToExpand();

        // Check that the expanded bar is showing the correct image and caption.
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(mActivityTestRule.getActivity().getResources().getString(
                                    ContextualSearchCaptionControl.EXPANED_CAPTION_ID),
                barControl.getCaptionText());
        Assert.assertEquals(0.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Go back to peeking.
        swipePanelDown();
        waitForPanelToPeek();

        // Assert that the quick action data is showing.
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(mActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        // TODO(donnd): figure out why we get ~0.65 on Oreo rather than 1. https://crbug.com/818515.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);
        } else {
            Assert.assertTrue(0.5f < imageControl.getCustomImageVisibilityPercentage());
        }
    }

    /**
     * Tests that an intent is sent when the bar is tapped and a quick action is available.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionIntent() throws InterruptedException, TimeoutException {
        // Add a new filter to the activity monitor that matches the intent that should be fired.
        IntentFilter quickActionFilter = new IntentFilter(Intent.ACTION_VIEW);
        quickActionFilter.addDataScheme("tel");
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(quickActionFilter,
                        new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);

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
        clickPanelBar();

        // Assert that an intent was fired.
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests that the current tab is navigated to the quick action URI for
     * QuickActionCategory#WEBSITE.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionUrl() throws InterruptedException, TimeoutException {
        final String testUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        // Simulate a tap to show the Bar, then set the quick action data.
        simulateTapSearch("search");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPanel.onSearchTermResolved("search", null, testUrl, QuickActionCategory.WEBSITE);
            }
        });

        // Tap on the portion of the bar that should trigger the quick action.
        clickPanelBar();

        // Assert that the URL was loaded.
        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), testUrl);
    }

    /**
     * Tests accessibility mode: Tap and Long-press don't activate CS.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAccesibilityMode() throws InterruptedException, TimeoutException {
        mManager.onAccessibilityModeChanged(true);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoContentViewCore();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoContentViewCore();
        assertNoSearchesLoaded();
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on Tap that Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAllInternalStatesVisitedResolvingTap()
            throws InterruptedException, TimeoutException {
        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a tap that resolves to show the Bar.
        simulateTapSearch("search");
        Assert.assertEquals(
                InternalState.SHOWING_TAP_SEARCH, internalStateControllerWrapper.getState());

        Assert.assertEquals(internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                ContextualSearchInternalStateControllerWrapper.EXPECTED_TAP_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on a Long-press.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAllInternalStatesVisitedLongpress()
            throws InterruptedException, TimeoutException {
        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a Long-press to show the Bar.
        simulateLongPressSearch("search");

        Assert.assertEquals(internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTriggeringContextualSearchHidesFindInPageOverlay() throws Exception {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.find_in_page_id);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                FindToolbar findToolbar =
                        (FindToolbar) mActivityTestRule.getActivity().findViewById(
                                R.id.find_toolbar);
                return findToolbar != null && findToolbar.isShown() && !findToolbar.isAnimating();
            }
        });

        // Don't type anything to Find because that may cause scrolling which makes clicking in the
        // page flaky.

        View findToolbar = mActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
        Assert.assertTrue(findToolbar.isShown());

        simulateTapSearch("search");

        waitForPanelToPeek();
        Assert.assertFalse(
                "Find Toolbar should no longer be shown once Contextual Search Panel appeared",
                findToolbar.isShown());
    }

    /**
     * Tests that Contextual Search is suppressed on long-press only when Smart Selection is
     * enabled, and that the Observers always get notified that text was selected.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSmartSelectSuppressesAndNotifiesObservers()
            throws InterruptedException, TimeoutException {
        // Mark the user undecided so we won't allow sending surroundings.
        mPolicy.overrideDecidedStateForTesting(false);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        mFakeServer.reset();

        longPressNodeWithoutWaiting("search");
        waitForSelectActionBarVisible();
        waitForPanelToPeek();
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());
        mManager.removeObserver(observer);

        tapBasePageToClosePanel();

        // Tell the ContextualSearchManager that Smart Selection is enabled.
        mManager.suppressContextualSearchForSmartSelection(true);
        observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        mFakeServer.reset();

        longPressNodeWithoutWaiting("search");
        waitForSelectActionBarVisible();
        assertPanelClosedOrUndefined();
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that expanding the selection during a Search Term Resolve notifies the observers before
     * and after the expansion.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNotifyObserversOnExpandSelection()
            throws InterruptedException, TimeoutException {
        mPolicy.overrideDecidedStateForTesting(true);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);

        simulateSlowResolveSearch("states");
        simulateSlowResolveFinished();
        closePanel();

        Assert.assertEquals("States".length(), observer.getFirstShownLength());
        Assert.assertEquals("United States".length(), observer.getLastShownLength());
        Assert.assertEquals(2, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /** Asserts that the given value is either 1 or 2.  Helpful for flaky tests. */
    private void assertValueIs1or2(int value) {
        if (value != 1) Assert.assertEquals(2, value);
    }

    /**
     * Tests a second Tap: a Tap on an existing tap-selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSecondTap() throws InterruptedException, TimeoutException {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);

        clickWordNode("search");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        clickNode("search");
        waitForSelectActionBarVisible();
        closePanel();

        // Sometimes we get an additional Show notification on the second Tap, but not reliably in
        // tests.  See crbug.com/776541.
        assertValueIs1or2(observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests a second Tap when Smart Selection is enabled.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSecondTapWithSmartSelection() throws InterruptedException, TimeoutException {
        mManager.suppressContextualSearchForSmartSelection(true);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);

        clickWordNode("search");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        clickNode("search");
        waitForSelectActionBarVisible();

        // Second Tap closes the panel automatically when Smart Selection is active.
        waitForPanelToClose();

        // Sometimes we get an additional Show notification on the second Tap, but not reliably in
        // tests.  See crbug.com/776541.
        assertValueIs1or2(observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests Tab reparenting.  When a tab moves from one activity to another the
     * ContextualSearchTabHelper should detect the change and handle gestures for it too.  This
     * happens with multiwindow modes.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testTabReparenting() throws InterruptedException, TimeoutException {
        // Move our "tap_test" tab to another activity.
        final ChromeActivity ca = mActivityTestRule.getActivity();
        int testTabId = ca.getActivityTab().getId();
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), ca,
                R.id.move_to_other_window_menu_id);

        // Wait for the second activity to start up and be ready for interaction.
        final ChromeTabbedActivity2 activity2 = waitForSecondChromeTabbedActivity();
        waitForTabs("CTA2", activity2, 1, testTabId);

        // Tap on a word and wait for the selection to be established.
        DOMUtils.clickNode(activity2.getActivityTab().getContentViewCore(), "search");
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String selection = activity2.getContextualSearchManager()
                                           .getSelectionController()
                                           .getSelectedText();
                return selection != null && selection.equals("Search");
            }
        });
    }
}
