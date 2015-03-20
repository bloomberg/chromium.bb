// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.net.Uri;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.ChromeSwitches;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.AnimationHelper;
import org.chromium.chrome.browser.infobar.AppBannerInfoBar;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.ArrayList;

/**
 * Tests the app banners.
 */
@CommandLineFlags.Add(ChromeSwitches.ENABLE_APP_INSTALL_ALERTS)
public class AppBannerManagerTest extends ChromeShellTestBase {
    private static final String NATIVE_APP_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/banners/native_app_test.html");

    private static final String NATIVE_ICON_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/banners/native_app_test.png");

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String WEB_APP_URL =
            TestHttpServerClient.getUrl("chrome/test/data/banners/manifest_test_page.html");

    private static final String WEB_APP_TITLE = "Manifest test app";

    private static class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;

        @Override
        protected void getAppDetailsAsynchronously(
                Observer observer, String url, String packageName, int iconSize) {
            mNumRetrieved += 1;
            mObserver = observer;
            mAppData = new AppData(url, packageName);
            mAppData.setPackageInfo(
                    NATIVE_APP_TITLE, NATIVE_ICON_URL, 4.5f, "Install this", null, null);
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onAppDetailsRetrieved(mAppData);
                }
            });
        }

        @Override
        public void destroy() {
        }
    }

    private static class InfobarListener implements InfoBarContainer.InfoBarAnimationListener {
        private boolean mDoneAnimating;

        @Override
        public void notifyAnimationFinished(int animationType) {
            if (animationType == AnimationHelper.ANIMATION_TYPE_SHOW) mDoneAnimating = true;
        }
    }

    private MockAppDetailsDelegate mDetailsDelegate;

    @Override
    protected void setUp() throws Exception {
        mDetailsDelegate = new MockAppDetailsDelegate();
        AppBannerManager.setAppDetailsDelegate(mDetailsDelegate);
        AppBannerManager.setIsEnabledForTesting(true);
        clearAppData();

        super.setUp();

        launchChromeShellWithUrl("about:blank");
        assertTrue(waitForActiveShellToBeDoneLoading());
        AppBannerManager.disableSecureSchemeCheckForTesting();
    }

    private boolean waitUntilNoInfoBarsExist() throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InfoBarContainer container = getActivity().getActiveTab().getInfoBarContainer();
                return container.getInfoBars().size() == 0;
            }
        });
    }

    private boolean waitUntilAppDetailsRetrieved(final int numExpected) throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActiveTab().getAppBannerManagerForTesting();
                return mDetailsDelegate.mNumRetrieved == numExpected
                        && !manager.isFetcherActiveForTesting();
            }
        });
    }

    private boolean waitUntilAppBannerInfoBarAppears(final String title) throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InfoBarContainer container = getActivity().getActiveTab().getInfoBarContainer();
                ArrayList<InfoBar> infobars = container.getInfoBars();
                if (infobars.size() != 1) return false;
                if (!(infobars.get(0) instanceof AppBannerInfoBar)) return false;

                TextView textView =
                        (TextView) infobars.get(0).getContentWrapper().findViewById(R.id.app_name);
                if (textView == null) return false;
                return TextUtils.equals(textView.getText(), title);
            }
        });
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testBannerAppears() throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(2));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBannerAppearsThenDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(2));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));

        // Revisit the page to make the banner go away, but don't explicitly dismiss it.
        // This hides the banner for a few months.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(3));
        assertTrue(waitUntilNoInfoBarsExist());

        // Wait a month until revisiting the page.
        AppBannerManager.setTimeDeltaForTesting(31);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(4));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(32);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(5));
        assertTrue(waitUntilNoInfoBarsExist());

        // Wait two months until revisiting the page, which should pop up the banner.
        AppBannerManager.setTimeDeltaForTesting(61);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(6));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(62);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(7));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedBannerDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page.
        InfoBarContainer container = getActivity().getActiveTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(2));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));

        // Explicitly dismiss the banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        }));
        ArrayList<InfoBar> infobars = container.getInfoBars();
        View close = infobars.get(0).getContentWrapper().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        assertTrue(waitUntilNoInfoBarsExist());

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(3));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(62);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(4));
        assertTrue(waitUntilNoInfoBarsExist());

        // Waiting three months should allow banners to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(5));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(92);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(6));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBitmapFetchersCanOverlapWithoutCrashing() throws Exception {
        // Visit a site that requests a banner rapidly and repeatedly.
        for (int i = 1; i <= 10; i++) {
            assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                    new TabLoadObserver(getActivity().getActiveTab(), NATIVE_APP_URL)));

            final Integer iteration = Integer.valueOf(i);
            assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mDetailsDelegate.mNumRetrieved == iteration;
                }
            }));
        }
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testWebAppBannerAppears() throws Exception {
        // Create a Tab that doesn't have the AppBannerManager enabled.  This prevents race
        // conditions between service worker activation and AppBannerManager getting triggered.
        // This race condition is a known problem, which is why the specs include wiggle room for
        // how many times a site must be visited.
        AppBannerManager.setIsEnabledForTesting(false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().createTab("about:blank");
            }
        });
        assertTrue(waitForActiveShellToBeDoneLoading());

        // Visit a site that can have a banner, then wait until the service worker is activated.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), WEB_APP_URL)));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String url = getActivity().getActiveTab().getUrl();
                Uri uri = Uri.parse(url);
                return TextUtils.equals(uri.getFragment(), "sw_activated");
            }
        }));
        AppBannerManager.setIsEnabledForTesting(true);

        // Revisit the site in a new tab, which will have the AppBannerManager enabled.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().createTab("about:blank");
            }
        });
        assertTrue(waitForActiveShellToBeDoneLoading());
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), WEB_APP_URL)));

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActiveTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page to show the banner.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActiveTab(), WEB_APP_URL)));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActiveTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilAppBannerInfoBarAppears(WEB_APP_TITLE));
    }
}
