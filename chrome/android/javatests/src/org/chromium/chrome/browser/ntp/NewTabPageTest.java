// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.graphics.Canvas;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.browser.suggestions.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.RenderUtils.ViewRenderer;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
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
@RetryOnFailure
public class NewTabPageTest extends ChromeTabbedActivityTestBase {

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    private static final String[] FAKE_MOST_VISITED_TITLES = new String[] { "Simple" };
    private static final String[] FAKE_MOST_VISITED_WHITELIST_ICON_PATHS = new String[] { "" };
    private static final int[] FAKE_MOST_VISITED_SOURCES = new int[] {NTPTileSource.TOP_SITES};

    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mTileGridLayout;
    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mSiteSuggestionUrls = new String[] {mTestServer.getURL(TEST_PAGE)};
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mTab = getActivity().getActivityTab();

        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    // Create FakeMostVisitedSites after starting the activity, since it depends on
                    // native code.
                    mMostVisitedSites = new FakeMostVisitedSites(mTab.getProfile());
                    mMostVisitedSites.setTileSuggestions(FAKE_MOST_VISITED_TITLES,
                            mSiteSuggestionUrls, FAKE_MOST_VISITED_WHITELIST_ICON_PATHS,
                            FAKE_MOST_VISITED_SOURCES);
                }
            });
        } catch (Throwable t) {
            fail(t.getMessage());
        }
        TileGroupDelegateImpl.setMostVisitedSitesForTests(mMostVisitedSites);

        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mTileGridLayout = (ViewGroup) mNtp.getView().findViewById(R.id.tile_grid_layout);
        assertEquals(mSiteSuggestionUrls.length, mTileGridLayout.getChildCount());
    }

    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @CommandLineFlags.Add("enable-features=NTPSnippets")
    public void testRender() throws IOException {
        ViewRenderer viewRenderer = new ViewRenderer(getActivity(),
                "chrome/test/data/android/render_tests", "NewTabPageTest");
        viewRenderer.renderAndCompare(mTileGridLayout, "most_visited");
        viewRenderer.renderAndCompare(mFakebox, "fakebox");
        viewRenderer.renderAndCompare(mNtp.getView().getRootView(), "new_tab_page");

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

        viewRenderer.renderAndCompare(mNtp.getView().getRootView(), "new_tab_page_scrolled");
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPSnippets")
    @UiThreadTest
    public void testThumbnailInvalidations() {
        captureThumbnail();
        assertFalse(mNtp.shouldCaptureThumbnail());

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

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testFocusFakebox() throws InterruptedException {
        int initialFakeboxTop = getFakeboxTop(mNtp);

        singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        int afterFocusFakeboxTop = getFakeboxTop(mNtp);
        assertTrue(afterFocusFakeboxTop < initialFakeboxTop);

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false);
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/593007")
    public void testSearchFromFakebox() throws InterruptedException {
        singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        getInstrumentation().sendStringSync(TEST_PAGE);
        LocationBarLayout locationBar =
                (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                KeyUtils.singleKeyEventView(getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);
            }
        });
    }

    /**
     * Tests clicking on a most visited item.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testClickMostVisitedItem() throws InterruptedException {
        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                View mostVisitedItem = mTileGridLayout.getChildAt(0);
                singleClickView(mostVisitedItem);
            }
        });
        assertEquals(mSiteSuggestionUrls[0], mTab.getUrl());
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @DisabledTest // Flaked on the try bot. http://crbug.com/543138
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInNewTab() throws InterruptedException {
        invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_NEW_TAB, false, mSiteSuggestionUrls[0]);
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInIncognitoTab() throws InterruptedException {
        invokeContextMenuAndOpenInANewTab(mTileGridLayout.getChildAt(0),
                ContextMenuManager.ID_OPEN_IN_INCOGNITO_TAB, true, mSiteSuggestionUrls[0]);
    }

    /**
     * Tests deleting a most visited item.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRemoveMostVisitedItem() {
        View mostVisitedItem = mTileGridLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mTileGridLayout.findViewsWithText(
                views, FAKE_MOST_VISITED_TITLES[0], View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, views.size());

        TestTouchUtils.longClickView(getInstrumentation(), mostVisitedItem);
        assertTrue(getInstrumentation().invokeContextMenuAction(
                getActivity(), ContextMenuManager.ID_REMOVE, 0));

        assertTrue(mMostVisitedSites.isUrlBlacklisted(mSiteSuggestionUrls[0]));
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testUrlFocusAnimationsDisabledOnLoad() throws InterruptedException {
        assertFalse(getUrlFocusAnimatonsDisabled());
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
                        assertTrue(getUrlFocusAnimatonsDisabled());
                    }
                });
            }
        });
        // Ensure it is still marked as disabled once the new page is fully loaded.
        assertTrue(getUrlFocusAnimatonsDisabled());
    }

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
                        assertTrue(delaySemaphore.tryAcquire(10, TimeUnit.SECONDS));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            };
            final String testPageUrl = webServer.setResponseWithRunnableAction(
                    "/ntp_test.html",
                    "<html><body></body></html>", null, delayAction);

            assertFalse(getUrlFocusAnimatonsDisabled());

            clickFakebox();
            UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
            OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
            typeInOmnibox(testPageUrl, false);
            LocationBarLayout locationBar =
                    (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
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
            KeyUtils.singleKeyEventView(getInstrumentation(), v, KeyEvent.KEYCODE_ENTER);

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
            assertFalse(getUrlFocusAnimatonsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Tests setting whether the search provider has a logo.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    @UiThreadTest
    public void testSetSearchProviderHasLogo() {
        NewTabPageView ntpView = mNtp.getNewTabPageView();
        View logoView = ntpView.findViewById(R.id.search_provider_logo);
        assertEquals(View.VISIBLE, logoView.getVisibility());
        ntpView.setSearchProviderHasLogo(false);
        assertEquals(View.GONE, logoView.getVisibility());
        ntpView.setSearchProviderHasLogo(true);
        assertEquals(View.VISIBLE, logoView.getVisibility());
    }

    /**
     * Tests setting whether the search provider has a logo when the condensed UI is enabled.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPCondensedLayout")
    @UiThreadTest
    public void testSetSearchProviderHasLogoCondensedUi() {
        NewTabPageView ntpView = mNtp.getNewTabPageView();
        View logoView = ntpView.findViewById(R.id.search_provider_logo);
        assertEquals(View.GONE, logoView.getVisibility());
        ntpView.setSearchProviderHasLogo(false);
        assertEquals(View.GONE, logoView.getVisibility());
        ntpView.setSearchProviderHasLogo(true);
        assertEquals(View.GONE, logoView.getVisibility());
    }

    /**
     * Verifies that the placeholder is only shown when there are no tile suggestions and the search
     * provider has no logo.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testPlaceholder() {
        final NewTabPageView ntpView = mNtp.getNewTabPageView();
        final View logoView = ntpView.findViewById(R.id.search_provider_logo);
        final View searchBoxView = ntpView.findViewById(R.id.search_box);

        // Initially, the logo is visible, the search box is visible, there is one tile suggestion,
        // and the placeholder has not been inflated yet.
        assertEquals(View.VISIBLE, logoView.getVisibility());
        assertEquals(View.VISIBLE, searchBoxView.getVisibility());
        assertEquals(1, mTileGridLayout.getChildCount());
        assertNull(ntpView.getPlaceholder());

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.setSearchProviderHasLogo(false);
                assertEquals(View.GONE, logoView.getVisibility());
                assertEquals(View.GONE, searchBoxView.getVisibility());
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
        assertNotNull(ntpView.getPlaceholder());
        assertEquals(View.VISIBLE, ntpView.getPlaceholder().getVisibility());

        // Once the search provider has a logo again, the logo and search box are shown again and
        // the placeholder is hidden.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ntpView.setSearchProviderHasLogo(true);
                assertEquals(View.VISIBLE, logoView.getVisibility());
                assertEquals(View.VISIBLE, searchBoxView.getVisibility());
                assertEquals(View.GONE, ntpView.getPlaceholder().getVisibility());
            }
        });
    }

    private void assertThumbnailInvalidAndRecapture() {
        assertTrue(mNtp.shouldCaptureThumbnail());
        captureThumbnail();
        assertFalse(mNtp.shouldCaptureThumbnail());
    }

    private void captureThumbnail() {
        Canvas canvas = new Canvas();
        mNtp.captureThumbnail(canvas);
    }

    private boolean getUrlFocusAnimatonsDisabled() {
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
                return getUrlFocusAnimatonsDisabled();
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
        singleClickView(fakebox);
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
