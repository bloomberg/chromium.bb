// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.ChromeModernDesign;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests related to displaying contextual suggestions in a bottom sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BOTTOM_SHEET)
@ChromeModernDesign.Enable
public class ContextualSuggestionsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ContextualSuggestionsDependenciesRule mContextualSuggestionsDeps =
            new ContextualSuggestionsDependenciesRule();
    @Rule
    public TestRule mChromeModernDesignStateRule = new ChromeModernDesign.Processor();
    @Rule
    public TestRule mFeaturesProcessor = new Features.InstrumentationProcessor();

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    private EmbeddedTestServer mTestServer;
    private ContextualSuggestionsCoordinator mCoordinator;
    private ContextualSuggestionsMediator mMediator;
    private ContextualSuggestionsModel mModel;
    private BottomSheet mBottomSheet;

    @Before
    public void setUp() throws Exception {
        mContextualSuggestionsDeps.getFactory().suggestionsSource =
                new FakeContextualSuggestionsSource();
        FetchHelper.setDisableDelayForTesting(true);

        FakeEnabledStateMonitor stateMonitor = new FakeEnabledStateMonitor();
        mContextualSuggestionsDeps.getFactory().enabledStateMonitor = new FakeEnabledStateMonitor();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator =
                    mActivityTestRule.getActivity().getContextualSuggestionsCoordinatorForTesting();
            mMediator = mCoordinator.getMediatorForTesting();
            mModel = mCoordinator.getModelForTesting();
            stateMonitor.setObserver(mMediator);
        });

        mBottomSheet = mActivityTestRule.getActivity().getBottomSheet();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        FetchHelper.setDisableDelayForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenContextualSuggestionsBottomSheet() {
        assertTrue("Bottom sheet should contain suggestions content",
                mBottomSheet.getCurrentSheetContent()
                                instanceof ContextualSuggestionsBottomSheetContent);
        assertEquals("Sheet should still be hidden.", BottomSheet.SHEET_STATE_HIDDEN,
                mBottomSheet.getSheetState());
        assertEquals("Title text should be set.",
                FakeContextualSuggestionsSource.TEST_TOOLBAR_TITLE, mModel.getTitle());

        assertEquals("Cluster list should be set.",
                (int) FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT,
                mModel.getClusterList().getItemCount());

        ContextualSuggestionsBottomSheetContent content =
                (ContextualSuggestionsBottomSheetContent) mBottomSheet.getCurrentSheetContent();
        RecyclerView recyclerView = (RecyclerView) content.getContentView();
        assertEquals("RecyclerView should be empty.", 0, recyclerView.getChildCount());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.showContentInSheetForTesting();
            mBottomSheet.endAnimations();
        });

        assertEquals("Sheet should be peeked.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertNull("RecyclerView should still be empty.", recyclerView.getAdapter());

        openSheet();

        assertEquals("RecyclerView should have content.",
                (int) FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT,
                recyclerView.getAdapter().getItemCount());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testCloseFromPeek() throws InterruptedException, TimeoutException {
        forceShowSuggestions();
        simulateClickOnCloseButton();
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testCloseFromOpen() throws InterruptedException, TimeoutException {
        forceShowSuggestions();
        openSheet();
        simulateClickOnCloseButton();
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testTriggerMultipleTimes() throws InterruptedException, TimeoutException {
        // Show one time and close.
        forceShowSuggestions();
        openSheet();
        simulateClickOnCloseButton();

        // Show a second time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMediator.requestSuggestions("http://www.testurl.com"));
        forceShowSuggestions();
        openSheet();
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenSuggestion() throws InterruptedException, TimeoutException {
        forceShowSuggestions();
        openSheet();

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        TestWebContentsObserver webContentsObserver = new TestWebContentsObserver(
                mActivityTestRule.getActivity().getActivityTab().getWebContents());

        int callCount = webContentsObserver.getOnPageStartedHelper().getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            holder.itemView.performClick();
        });

        webContentsObserver.getOnPageStartedHelper().waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        assertEquals("Tab URL should match snippet URL", expectedUrl,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
        assertFalse("Sheet should be closed.", mBottomSheet.isSheetOpen());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenArticleInNewTab() throws Exception {
        forceShowSuggestions();
        openSheet();

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule, holder.itemView,
                ContextMenuManager.ID_OPEN_IN_NEW_TAB, false, expectedUrl);

        assertEquals("Sheet should still be opened.", BottomSheet.SHEET_STATE_FULL,
                mBottomSheet.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenSuggestionInNewTabIncognito() throws Exception {
        forceShowSuggestions();
        openSheet();

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule, holder.itemView,
                ContextMenuManager.ID_OPEN_IN_INCOGNITO_TAB, true, expectedUrl);

        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        assertFalse("Sheet should be closed.", mBottomSheet.isSheetOpen());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testEnterTabSwitcher() throws Exception {
        forceShowSuggestions();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManager().showOverview(false);
            mBottomSheet.endAnimations();
        });

        assertEquals("Sheet should be hidden.", BottomSheet.SHEET_STATE_HIDDEN,
                mBottomSheet.getSheetState());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManager().hideOverview(false);
            mBottomSheet.endAnimations();
        });

        assertEquals("Sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testMenuButton() throws Exception {
        forceShowSuggestions();
        View title =
                mBottomSheet.getCurrentSheetContent().getToolbarView().findViewById(R.id.title);
        View menuButton =
                mBottomSheet.getCurrentSheetContent().getToolbarView().findViewById(R.id.more);

        int titleWidth = title.getWidth();
        // Menu button is not visible on peek state.
        assertEquals(View.GONE, menuButton.getVisibility());
        assertEquals(0, menuButton.getWidth());

        // Menu button is visible after sheet is opened.
        openSheet();
        assertEquals(View.VISIBLE, menuButton.getVisibility());

        // Title view should be resized.
        ViewUtils.waitForStableView(menuButton);
        assertNotEquals(0, menuButton.getWidth());
        assertEquals(titleWidth, title.getWidth() + menuButton.getWidth());
    }

    private void forceShowSuggestions() throws InterruptedException, TimeoutException {
        assertEquals("Model has incorrect number of items.",
                (int) FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT,
                mModel.getClusterList().getItemCount());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.showContentInSheetForTesting();
            mBottomSheet.endAnimations();

            assertEquals("Sheet should be peeked.", BottomSheet.SHEET_STATE_PEEK,
                    mBottomSheet.getSheetState());
            assertTrue("Bottom sheet should contain suggestions content",
                    mBottomSheet.getCurrentSheetContent()
                                    instanceof ContextualSuggestionsBottomSheetContent);
        });
    }

    private void simulateClickOnCloseButton() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.getCurrentSheetContent()
                    .getToolbarView()
                    .findViewById(R.id.close_button)
                    .performClick();
            mBottomSheet.endAnimations();
        });

        assertEquals("Sheet should be hidden.", BottomSheet.SHEET_STATE_HIDDEN,
                mBottomSheet.getSheetState());
        assertNull("Bottom sheet contents should be null.", mBottomSheet.getCurrentSheetContent());
    }

    private void openSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_FULL, false));
    }

    private SnippetArticleViewHolder getFirstSuggestionViewHolder() {
        ContextualSuggestionsBottomSheetContent content =
                (ContextualSuggestionsBottomSheetContent) mBottomSheet.getCurrentSheetContent();
        RecyclerView recyclerView = (RecyclerView) content.getContentView();

        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);

        return (SnippetArticleViewHolder) recyclerView.findViewHolderForAdapterPosition(2);
    }
}
