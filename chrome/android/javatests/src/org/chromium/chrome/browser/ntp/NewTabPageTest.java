// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the native android New Tab Page.
 */
public class NewTabPageTest extends ChromeTabbedActivityTestBase {

    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/navigate/simple.html");

    private static final String[] FAKE_MOST_VISITED_TITLES = new String[] { "Simple" };
    private static final String[] FAKE_MOST_VISITED_URLS = new String[] { TEST_PAGE };

    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mMostVisitedLayout;
    private FakeMostVisitedSites mFakeMostVisitedSites;

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
                    mFakeMostVisitedSites = new FakeMostVisitedSites(mTab.getProfile(),
                            FAKE_MOST_VISITED_TITLES, FAKE_MOST_VISITED_URLS);
                }
            });
        } catch (Throwable t) {
            fail(t.getMessage());
        }
        NewTabPage.setMostVisitedSitesForTests(mFakeMostVisitedSites);

        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mMostVisitedLayout = (ViewGroup) mNtp.getView().findViewById(R.id.most_visited_layout);
        assertEquals(FAKE_MOST_VISITED_URLS.length, mMostVisitedLayout.getChildCount());
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
        assertTrue(OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true));
        int afterFocusFakeboxTop = getFakeboxTop(mNtp);
        assertTrue(afterFocusFakeboxTop < initialFakeboxTop);

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
        assertTrue(OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false));
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testSearchFromFakebox() throws InterruptedException {
        singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertTrue(OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true));

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
                View mostVisitedItem = mMostVisitedLayout.getChildAt(0);
                singleClickView(mostVisitedItem);
            }
        });
        assertEquals(FAKE_MOST_VISITED_URLS[0], mTab.getUrl());
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @DisabledTest // Flaked on the try bot. http://crbug.com/543138
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInNewTab() throws InterruptedException {
        invokeContextMenuAndOpenInANewTab(mMostVisitedLayout.getChildAt(0),
                NewTabPage.ID_OPEN_IN_NEW_TAB, false, FAKE_MOST_VISITED_URLS[0]);
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testOpenMostVisitedItemInIncognitoTab() throws InterruptedException {
        invokeContextMenuAndOpenInANewTab(mMostVisitedLayout.getChildAt(0),
                NewTabPage.ID_OPEN_IN_INCOGNITO_TAB, true, FAKE_MOST_VISITED_URLS[0]);
    }

    /**
     * Tests deleting a most visited item.
     */
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRemoveMostVisitedItem() {
        View mostVisitedItem = mMostVisitedLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<View>();
        mMostVisitedLayout.findViewsWithText(views, FAKE_MOST_VISITED_TITLES[0],
                View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, views.size());

        TestTouchUtils.longClickView(getInstrumentation(), mostVisitedItem);
        assertTrue(getInstrumentation().invokeContextMenuAction(getActivity(),
                NewTabPage.ID_REMOVE, 0));

        assertTrue(mFakeMostVisitedSites.isUrlBlacklisted(FAKE_MOST_VISITED_URLS[0]));
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
                        mTab.loadUrl(new LoadUrlParams(TEST_PAGE, pageTransition));
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
    @Feature({"NewTagPage"})
    public void testUrlFocusAnimationsEnabledOnFailedLoad() throws Exception {
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

            assertTrue(waitForUrlFocusAnimationsDisabledState(true));
            assertTrue(waitForTabLoading());

            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    mTab.stopLoading();
                }
            });
            assertTrue(waitForUrlFocusAnimationsDisabledState(false));
            delaySemaphore.release();
            loadedCallback.waitForCallback(0);
            assertFalse(getUrlFocusAnimatonsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    private boolean getUrlFocusAnimatonsDisabled() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mNtp.getNewTabPageView().urlFocusAnimationsDisabled();
            }
        });
    }

    private boolean waitForUrlFocusAnimationsDisabledState(final boolean disabled)
            throws InterruptedException {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getUrlFocusAnimatonsDisabled() == disabled;
            }
        });
    }

    private boolean waitForTabLoading() throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTab.isLoading();
            }
        });
    }

    private void waitForFakeboxFocusAnimationComplete(NewTabPage ntp) throws InterruptedException {
        waitForUrlFocusPercent(ntp, 1f);
    }

    private void waitForFakeboxUnfocusAnimationComplete(NewTabPage ntp)
            throws InterruptedException {
        waitForUrlFocusPercent(ntp, 0f);
    }

    private void waitForUrlFocusPercent(final NewTabPage ntp, final float percent)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ntp.getNewTabPageView().getUrlFocusChangeAnimationPercent() == percent;
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
    private void waitForFakeboxTopPosition(final NewTabPage ntp, final int position)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getFakeboxTop(ntp) == position;
            }
        }));
    }
}
