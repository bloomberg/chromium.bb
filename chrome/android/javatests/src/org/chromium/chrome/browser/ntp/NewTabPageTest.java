// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Canvas;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.cards.SuggestionsSection;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.TileSectionType;
import org.chromium.chrome.browser.suggestions.TileSource;
import org.chromium.chrome.browser.suggestions.TileTitleSource;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.ChromeModernDesign;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.PageTransition;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the native android New Tab Page.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DisableFeatures({"NetworkPrediction", ChromeFeatureList.NTP_ARTICLE_SUGGESTIONS_EXPANDABLE_HEADER})
@RetryOnFailure
public class NewTabPageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    @Rule
    public MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    @Rule
    public ChromeModernDesign.Processor mChromeModernProcessor = new ChromeModernDesign.Processor();

    @Rule
    public TestRule mFeatureRule = new Features.InstrumentationProcessor();

    @ParameterAnnotations.MethodParameter("Modern")
    private static List<ParameterSet> sMethodParamModern =
            Arrays.asList(new ParameterSet().value(false).name("DisableChromeModern"),
                    new ParameterSet().value(true).name("EnableChromeModern"));

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;

    @ParameterAnnotations.UseMethodParameterBefore("Modern")
    public void setupModernDesign(boolean enabled) {
        mChromeModernProcessor.setPrefs(enabled);

        if (enabled) mRenderTestRule.setVariantPrefix("modern");
    }

    @ParameterAnnotations.UseMethodParameterAfter("Modern")
    public void teardownModernDesign(boolean enabled) {
        mChromeModernProcessor.clearTestState();
    }

    @Before
    public void setUp() throws Exception {
        // Disable peeking card animation.
        ChromePreferenceManager.getInstance().setNewTabPageFirstCardAnimationRunCount(100);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mSiteSuggestions = new ArrayList<>();
        mSiteSuggestions.add(new SiteSuggestion("0 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#0",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("1 WHITELIST", mTestServer.getURL(TEST_PAGE) + "#1",
                "/test.png", TileTitleSource.UNKNOWN, TileSource.WHITELIST,
                TileSectionType.PERSONALIZED, new Date()));
        mSiteSuggestions.add(new SiteSuggestion("2 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#2",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("3 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#3",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("4 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#4",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("5 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#5",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("6 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#6",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        mSiteSuggestions.add(new SiteSuggestion("7 TOP_SITES", mTestServer.getURL(TEST_PAGE) + "#7",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));

        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter("Modern")
    public void testRender(boolean modern) throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        RenderTestRule.sanitize(mNtp.getView());
        mRenderTestRule.render(mTileGridLayout, "most_visited");
        mRenderTestRule.render(mFakebox, "fakebox");
        mRenderTestRule.render(mNtp.getView().getRootView(), "new_tab_page");

        RecyclerViewTestUtils.scrollToBottom(mNtp.getNewTabPageView().getRecyclerView());
        mRenderTestRule.render(mNtp.getView().getRootView(), "new_tab_page_scrolled");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testThumbnailInvalidations() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                captureThumbnail();
                Assert.assertFalse(mNtp.shouldCaptureThumbnail());

                // Check that we invalidate the thumbnail when the Recycler View is updated.
                NewTabPageRecyclerView recyclerView = mNtp.getNewTabPageView().getRecyclerView();

                recyclerView.getAdapter().notifyDataSetChanged();
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemChanged(0);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemInserted(0);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemMoved(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeChanged(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeInserted(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeRemoved(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRemoved(0);
                assertThumbnailInvalidAndRecapture();
            }
        });
    }

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testFocusFakebox() {
        int initialFakeboxTop = getFakeboxTop(mNtp);

        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        int afterFocusFakeboxTop = getFakeboxTop(mNtp);
        Assert.assertTrue(afterFocusFakeboxTop < initialFakeboxTop);

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false);
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/593007")
    public void testSearchFromFakebox() throws InterruptedException {
        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        InstrumentationRegistry.getInstrumentation().sendStringSync(TEST_PAGE);
        LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(), urlBar,
                        KeyEvent.KEYCODE_ENTER);
            }
        });
    }

    /**
     * Tests clicking on a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testClickMostVisitedItem() throws InterruptedException {
        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                View mostVisitedItem = mTileGridLayout.getChildAt(0);
                TouchCommon.singleClickView(mostVisitedItem);
            }
        });
        Assert.assertEquals(mSiteSuggestions.get(0).url, mTab.getUrl());
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @Test
    @DisabledTest // Flaked on the try bot. http://crbug.com/543138
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInNewTab() throws InterruptedException, ExecutionException {
        invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_NEW_TAB, false, mSiteSuggestions.get(0).url);
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInIncognitoTab()
            throws InterruptedException, ExecutionException {
        invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_INCOGNITO_TAB, true, mSiteSuggestions.get(0).url);
    }

    /**
     * Tests deleting a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRemoveMostVisitedItem() throws ExecutionException {
        SiteSuggestion testSite = mSiteSuggestions.get(0);
        View mostVisitedItem = mTileGridLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mTileGridLayout.findViewsWithText(views, testSite.title, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), ContextMenuManager.ID_REMOVE, 0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlacklisted(testSite.url));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testUrlFocusAnimationsDisabledOnLoad() throws InterruptedException {
        Assert.assertFalse(getUrlFocusAnimationsDisabled());
        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        int pageTransition =
                                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                        mTab.loadUrl(new LoadUrlParams(mTestServer.getURL(TEST_PAGE),
                                pageTransition));
                        // It should be disabled as soon as a load URL is triggered.
                        Assert.assertTrue(getUrlFocusAnimationsDisabled());
                    }
                });
            }
        });
        // Ensure it is still marked as disabled once the new page is fully loaded.
        Assert.assertTrue(getUrlFocusAnimationsDisabled());
    }

    @Test
    @LargeTest
    @Feature({"NewTabPage"})
    public void testUrlFocusAnimationsEnabledOnFailedLoad() throws Exception {
        // TODO(jbudorick): switch this to EmbeddedTestServer.
        TestWebServer webServer = TestWebServer.start();
        try {
            final Semaphore delaySemaphore = new Semaphore(0);
            Runnable delayAction = new Runnable() {
                @Override
                public void run() {
                    try {
                        Assert.assertTrue(delaySemaphore.tryAcquire(10, TimeUnit.SECONDS));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            };
            final String testPageUrl = webServer.setResponseWithRunnableAction(
                    "/ntp_test.html",
                    "<html><body></body></html>", null, delayAction);

            Assert.assertFalse(getUrlFocusAnimationsDisabled());

            clickFakebox();
            UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
            OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
            mActivityTestRule.typeInOmnibox(testPageUrl, false);
            LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

            final CallbackHelper loadedCallback = new CallbackHelper();
            mTab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadFinished(Tab tab) {
                    loadedCallback.notifyCalled();
                    tab.removeObserver(this);
                }
            });

            final View v = urlBar;
            KeyUtils.singleKeyEventView(
                    InstrumentationRegistry.getInstrumentation(), v, KeyEvent.KEYCODE_ENTER);

            waitForUrlFocusAnimationsDisabledState(true);
            waitForTabLoading();

            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    mTab.stopLoading();
                }
            });
            waitForUrlFocusAnimationsDisabledState(false);
            delaySemaphore.release();
            loadedCallback.waitForCallback(0);
            Assert.assertFalse(getUrlFocusAnimationsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Tests setting whether the search provider has a logo.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testSetSearchProviderInfo() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageView ntpView = mNtp.getNewTabPageView();
                View logoView = ntpView.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                ntpView.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpView.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
            }
        });
    }

    /**
     * Tests setting whether the search provider has a logo when the condensed UI is enabled.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testSetSearchProviderInfoCondensedUi() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageView ntpView = mNtp.getNewTabPageView();
                View logoView = ntpView.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                ntpView.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpView.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
            }
        });
    }

    /**
     * Verifies that the placeholder is only shown when there are no tile suggestions and the search
     * provider has no logo.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testPlaceholder() {
        final NewTabPageView ntpView = mNtp.getNewTabPageView();
        final View logoView = ntpView.findViewById(R.id.search_provider_logo);
        final View searchBoxView = ntpView.findViewById(R.id.search_box);

        // Initially, the logo is visible, the search box is visible, there is one tile suggestion,
        // and the placeholder has not been inflated yet.
        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
        Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
        Assert.assertEquals(8, mTileGridLayout.getChildCount());
        Assert.assertNull(ntpView.getPlaceholder());

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                Assert.assertEquals(View.GONE, searchBoxView.getVisibility());

                mMostVisitedSites.setTileSuggestions(new String[] {});

                ntpView.getTileGroup().onSwitchToForeground(false); // Force tile refresh.
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("The tile grid was not updated.") {
            @Override
            public boolean isSatisfied() {
                return mTileGridLayout.getChildCount() == 0;
            }
        });
        Assert.assertNotNull(ntpView.getPlaceholder());
        Assert.assertEquals(View.VISIBLE, ntpView.getPlaceholder().getVisibility());

        // Once the search provider has a logo again, the logo and search box are shown again and
        // the placeholder is hidden.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
                Assert.assertEquals(View.GONE, ntpView.getPlaceholder().getVisibility());
            }
        });
    }

    @Test
    @SmallTest
    public void testRemoteSuggestionsEnabledByDefault() {
        Assert.assertTrue(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add("disable-features=NTPArticleSuggestions")
    public void testRemoteSuggestionsEnabledWhenFeatureDisabled() {
        // Verifies crash from https://crbug.com/742056.
        Assert.assertFalse(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @Policies.Add(@Policies.Item(key = "NTPContentSuggestionsEnabled", string = "false"))
    public void testRemoteSuggestionsEnabledWhenDisabledByPolicy() {
        Assert.assertFalse(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    @EnableFeatures(ChromeFeatureList.NTP_ARTICLE_SUGGESTIONS_EXPANDABLE_HEADER)
    public void testArticleExpandableHeaderOnMultipleTabs() throws Exception {
        // Open a new tab.
        SuggestionsSection firstSection = getArticleSectionOnNewTab();
        SectionHeader firstHeader = firstSection.getHeaderItemForTesting();
        int firstTabId = mActivityTestRule.getActivity().getActivityTab().getId();
        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(firstSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());
        // Toggle header on the current tab.
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        Assert.assertTrue(firstHeader.isExpandable() && !firstHeader.isExpanded());
        Assert.assertEquals(1, firstSection.getItemCount());
        Assert.assertFalse(getPreferenceForExpandableHeader());

        // Open a second new tab.
        SuggestionsSection secondSection = getArticleSectionOnNewTab();
        SectionHeader secondHeader = secondSection.getHeaderItemForTesting();
        // Check header on the second tab is collapsed.
        Assert.assertTrue(secondHeader.isExpandable() && !secondHeader.isExpanded());
        Assert.assertEquals(1, secondSection.getItemCount());
        Assert.assertFalse(getPreferenceForExpandableHeader());

        // Toggle header on the second tab.
        onView(withId(R.id.header_title)).perform(click());
        // Check header on the second tab is expanded.
        Assert.assertTrue(secondHeader.isExpandable() && secondHeader.isExpanded());
        Assert.assertTrue(secondSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());
        // Go back to the first tab and check header is expanded.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), firstTabId);
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(firstSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "RenderTest"})
    @EnableFeatures(ChromeFeatureList.NTP_ARTICLE_SUGGESTIONS_EXPANDABLE_HEADER)
    public void testArticleExpandableHeaderAppearance() throws Exception {
        NewTabPage ntp =
                (NewTabPage) mActivityTestRule.getActivity().getActivityTab().getNativePage();
        RecyclerView recyclerView = ntp.getNewTabPageView().getRecyclerView();
        NewTabPageAdapter adapter = (NewTabPageAdapter) recyclerView.getAdapter();
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        View view = recyclerView.findViewHolderForAdapterPosition(
                adapter.getFirstHeaderPosition()).itemView;

        // Check header is expanded.
        mRenderTestRule.render(view, "expandable_header_expanded");
        // Toggle header on the current tab.
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        mRenderTestRule.render(view, "expandable_header_collapsed");
    }

    private void assertThumbnailInvalidAndRecapture() {
        Assert.assertTrue(mNtp.shouldCaptureThumbnail());
        captureThumbnail();
        Assert.assertFalse(mNtp.shouldCaptureThumbnail());
    }

    private void captureThumbnail() {
        Canvas canvas = new Canvas();
        mNtp.captureThumbnail(canvas);
    }

    private boolean getUrlFocusAnimationsDisabled() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mNtp.getNewTabPageView().urlFocusAnimationsDisabled();
            }
        });
    }

    private void waitForUrlFocusAnimationsDisabledState(boolean disabled) {
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(disabled, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return getUrlFocusAnimationsDisabled();
            }
        }));
    }

    private void waitForTabLoading() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTab.isLoading();
            }
        });
    }

    private void waitForFakeboxFocusAnimationComplete(NewTabPage ntp) {
        waitForUrlFocusPercent(ntp, 1f);
    }

    private void waitForUrlFocusPercent(final NewTabPage ntp, float percent) {
        CriteriaHelper.pollUiThread(Criteria.equals(percent, new Callable<Float>() {
            @Override
            public Float call() {
                return ntp.getNewTabPageView().getUrlFocusChangeAnimationPercent();
            }
        }));
    }

    private void clickFakebox() {
        View fakebox = mNtp.getView().findViewById(R.id.search_box);
        TouchCommon.singleClickView(fakebox);
    }

    /**
     * @return The position of the top of the fakebox relative to the window.
     */
    private int getFakeboxTop(final NewTabPage ntp) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                final View fakebox = ntp.getView().findViewById(R.id.search_box);
                int[] location = new int[2];
                fakebox.getLocationInWindow(location);
                return location[1];
            }
        });
    }

    /**
     * Waits until the top of the fakebox reaches the given position.
     */
    private void waitForFakeboxTopPosition(final NewTabPage ntp, int position) {
        CriteriaHelper.pollUiThread(Criteria.equals(position, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getFakeboxTop(ntp);
            }
        }));
    }

    /**
     * Long presses the view, selects an item from the context menu, and
     * asserts that a new tab is opened and is incognito if expectIncognito is true.
     * @param view The View to long press.
     * @param contextMenuItemId The context menu item to select on the view.
     * @param expectIncognito Whether the opened tab is expected to be incognito.
     * @param expectedUrl The expected url for the new tab.
     */
    private void invokeContextMenuAndOpenInANewTab(
            View view, int contextMenuItemId, boolean expectIncognito, final String expectedUrl)
            throws InterruptedException, ExecutionException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final TabModel tabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(expectIncognito);
        tabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                if (TextUtils.equals(expectedUrl, tab.getUrl())) {
                    createdCallback.notifyCalled();
                    tabModel.removeObserver(this);
                }
            }
        });

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }

        if (expectIncognito) {
            Assert.assertTrue(
                    mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        } else {
            Assert.assertFalse(
                    mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        }
    }

    private SuggestionsSection getArticleSectionOnNewTab() throws Exception {
        Tab tab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPage ntp = (NewTabPage) tab.getNativePage();
        NewTabPageAdapter adapter =
                (NewTabPageAdapter) ntp.getNewTabPageView().getRecyclerView().getAdapter();
        return adapter.getSectionListForTesting().getSection(KnownCategories.ARTICLES);
    }

    private boolean getPreferenceForExpandableHeader() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
}
