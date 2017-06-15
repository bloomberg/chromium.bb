// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.graphics.Canvas;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.browser.suggestions.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.TileSource;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;

import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the native android New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@RetryOnFailure
public class NewTabPageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    private static final String[] FAKE_MOST_VISITED_TITLES =
            new String[] {"TOP_SITES", "WHITELIST"};
    private static final String[] FAKE_MOST_VISITED_WHITELIST_ICON_PATHS =
            new String[] {"", "/test.png"};
    private static final int[] FAKE_MOST_VISITED_SOURCES =
            new int[] {TileSource.TOP_SITES, TileSource.WHITELIST};

    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mTileGridLayout;
    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());

        // The URLs may not contain duplicates.
        mSiteSuggestionUrls = new String[FAKE_MOST_VISITED_TITLES.length];
        for (int i = 0; i < mSiteSuggestionUrls.length; i++) {
            mSiteSuggestionUrls[i] = mTestServer.getURL(TEST_PAGE) + "#" + i;
        }

        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(FAKE_MOST_VISITED_TITLES, mSiteSuggestionUrls,
                FAKE_MOST_VISITED_WHITELIST_ICON_PATHS, FAKE_MOST_VISITED_SOURCES);
        TileGroupDelegateImpl.setMostVisitedSitesForTests(mMostVisitedSites);
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mTileGridLayout = (ViewGroup) mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestionUrls.length, mTileGridLayout.getChildCount());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        TileGroupDelegateImpl.setMostVisitedSitesForTests(null);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testRender() throws IOException {
        mRenderTestRule.render(mTileGridLayout, "most_visited");
        mRenderTestRule.render(mFakebox, "fakebox");
        mRenderTestRule.render(mNtp.getView().getRootView(), "new_tab_page");

        // Scroll to search bar
        final NewTabPageRecyclerView recyclerView = mNtp.getNewTabPageView().getRecyclerView();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                recyclerView.smoothScrollBy(0, mFakebox.getTop());
            }
        });

        CriteriaHelper.pollUiThread(new Criteria(){
            @Override
            public boolean isSatisfied() {
                return recyclerView.computeVerticalScrollOffset() == mFakebox.getTop();
            }
        });

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
        Assert.assertEquals(mSiteSuggestionUrls[0], mTab.getUrl());
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @Test
    @DisabledTest // Flaked on the try bot. http://crbug.com/543138
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInNewTab() throws InterruptedException {
        mActivityTestRule.invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_NEW_TAB, false, mSiteSuggestionUrls[0]);
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInIncognitoTab() throws InterruptedException {
        mActivityTestRule.invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_INCOGNITO_TAB, true, mSiteSuggestionUrls[0]);
    }

    /**
     * Tests deleting a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRemoveMostVisitedItem() {
        View mostVisitedItem = mTileGridLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mTileGridLayout.findViewsWithText(
                views, FAKE_MOST_VISITED_TITLES[0], View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.longClickView(InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), ContextMenuManager.ID_REMOVE, 0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlacklisted(mSiteSuggestionUrls[0]));
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
    public void testSetSearchProviderHasLogo() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageView ntpView = mNtp.getNewTabPageView();
                View logoView = ntpView.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                ntpView.setSearchProviderHasLogo(false);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpView.setSearchProviderHasLogo(true);
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
    @CommandLineFlags.Add("enable-features=NTPCondensedLayout")
    public void testSetSearchProviderHasLogoCondensedUi() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageView ntpView = mNtp.getNewTabPageView();
                View logoView = ntpView.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpView.setSearchProviderHasLogo(false);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpView.setSearchProviderHasLogo(true);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
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
        Assert.assertEquals(2, mTileGridLayout.getChildCount());
        Assert.assertNull(ntpView.getPlaceholder());

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.setSearchProviderHasLogo(false);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                Assert.assertEquals(View.GONE, searchBoxView.getVisibility());
            }
        });
        mMostVisitedSites.setTileSuggestions(
                new String[] {}, new String[] {}, new String[] {}, new int[] {});
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.getTileGroup().onSwitchToForeground(); // Force the tiles to be refreshed.
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
                ntpView.setSearchProviderHasLogo(true);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
                Assert.assertEquals(View.GONE, ntpView.getPlaceholder().getVisibility());
            }
        });
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
}
