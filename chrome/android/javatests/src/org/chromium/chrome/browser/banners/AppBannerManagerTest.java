// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.graphics.Bitmap;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.AnimationHelper;
import org.chromium.chrome.browser.infobar.AppBannerInfoBarAndroid;
import org.chromium.chrome.browser.infobar.AppBannerInfoBarDelegateAndroid;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the app banners.
 */
public class AppBannerManagerTest extends ChromeTabbedActivityTestBase {
    private static final String NATIVE_APP_URL =
            TestHttpServerClient.getUrl("chrome/test/data/banners/play_app_test_page.html");

    private static final String NATIVE_ICON_URL =
            TestHttpServerClient.getUrl("chrome/test/data/banners/launcher-icon-4x.png");

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_PACKAGE = "123456";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String NATIVE_APP_URL_WITH_MANIFEST_URL =
            TestHttpServerClient.getUrl("chrome/test/data/banners/play_app_url_test_page.html");

    private static final String WEB_APP_URL =
            TestHttpServerClient.getUrl("chrome/test/data/banners/manifest_test_page.html");

    private static final String WEB_APP_TITLE = "Manifest test app";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private static class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;
        private Intent mInstallIntent;
        private String mReferrer;

        @Override
        protected void getAppDetailsAsynchronously(
                Observer observer, String url, String packageName, String referrer, int iconSize) {
            mNumRetrieved += 1;
            mObserver = observer;
            mReferrer = referrer;
            mInstallIntent = new Intent(INSTALL_ACTION);

            mAppData = new AppData(url, packageName);
            mAppData.setPackageInfo(NATIVE_APP_TITLE, NATIVE_ICON_URL, 4.5f,
                    NATIVE_APP_INSTALL_TEXT, null, mInstallIntent);
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

    private static class TestPackageManager extends MockPackageManager {
        public boolean isInstalled = false;

        @Override
        public List<PackageInfo> getInstalledPackages(int flags) {
            List<PackageInfo> packages = new ArrayList<PackageInfo>();

            if (isInstalled) {
                PackageInfo info = new PackageInfo();
                info.packageName = NATIVE_APP_PACKAGE;
                packages.add(info);
            }

            return packages;
        }
    }

    private static class TestDataStorageFactory extends WebappDataStorage.Factory {
        public Bitmap mSplashImage;

        @Override
        public WebappDataStorage create(final Context context, final String webappId) {
            return new WebappDataStorageWrapper(context, webappId);
        }

        private class WebappDataStorageWrapper extends WebappDataStorage {

            public WebappDataStorageWrapper(Context context, String webappId) {
                super(context, webappId);
            }

            @Override
            public void updateSplashScreenImage(Bitmap splashScreenImage) {
                assertNull(mSplashImage);
                mSplashImage = splashScreenImage;
            }
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
    private TestPackageManager mPackageManager;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        mPackageManager = new TestPackageManager();
        AppBannerManager.setIsEnabledForTesting(true);
        AppBannerInfoBarDelegateAndroid.setPackageManagerForTesting(mPackageManager);

        super.setUp();

        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AppBannerManager.setAppDetailsDelegate(mDetailsDelegate);
            }
        });

        AppBannerManager.disableSecureSchemeCheckForTesting();

        // Navigations in this test are all of type ui::PAGE_TRANSITION_LINK, i.e. indirect.
        // Force indirect navigations to be worth the same as direct for testing.
        AppBannerManager.setEngagementWeights(1, 1);
    }

    private boolean waitUntilNoInfoBarsExist() throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
                return container.getInfoBars().size() == 0;
            }
        });
    }

    private boolean waitUntilAppDetailsRetrieved(final int numExpected) throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return mDetailsDelegate.mNumRetrieved == numExpected
                        && !manager.isFetcherActiveForTesting();
            }
        });
    }

    private boolean waitUntilAppBannerInfoBarAppears(final String title) throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
                ArrayList<InfoBar> infobars = container.getInfoBars();
                if (infobars.size() != 1) return false;
                if (!(infobars.get(0) instanceof AppBannerInfoBarAndroid)) return false;

                TextView textView =
                        (TextView) infobars.get(0).getContentWrapper().findViewById(R.id.app_name);
                if (textView == null) return false;
                return TextUtils.equals(textView.getText(), title);
            }
        });
    }

    private void runFullNativeInstallPathway(String url, String expectedReferrer) throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), url)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page to get the banner to appear.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), url)));
        assertTrue(waitUntilAppDetailsRetrieved(2));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        }));

        // Check that the button asks if the user wants to install the app.
        InfoBar infobar = container.getInfoBars().get(0);
        final Button button =
                (Button) infobar.getContentWrapper().findViewById(R.id.button_primary);
        assertEquals(NATIVE_APP_INSTALL_TEXT, button.getText());

        // Click the button to trigger the install.
        final ActivityMonitor activityMonitor = new ActivityMonitor(
                new IntentFilter(INSTALL_ACTION), new ActivityResult(Activity.RESULT_OK, null),
                true);
        getInstrumentation().addMonitor(activityMonitor);
        TouchCommon.singleClickView(button);

        // Wait for the infobar to register that the app is installing.
        final String installingText =
                getInstrumentation().getTargetContext().getString(R.string.app_banner_installing);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInstrumentation().checkMonitorHit(activityMonitor, 1)
                        && TextUtils.equals(button.getText(), installingText);
            }
        }));

        // Say that the package is installed.  Infobar should say that the app is ready to open.
        mPackageManager.isInstalled = true;
        final String openText =
                getInstrumentation().getTargetContext().getString(R.string.app_banner_open);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals(button.getText(), openText);
            }
        }));
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testFullNativeInstallPathwayFromId() throws Exception {
        runFullNativeInstallPathway(NATIVE_APP_URL, NATIVE_APP_BLANK_REFERRER);
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testFullNativeInstallPathwayFromUrl() throws Exception {
        runFullNativeInstallPathway(NATIVE_APP_URL_WITH_MANIFEST_URL, NATIVE_APP_REFERRER);
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBannerAppearsThenDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(2));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));

        // Revisit the page to make the banner go away, but don't explicitly dismiss it.
        // This hides the banner for a few months.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(3));
        assertTrue(waitUntilNoInfoBarsExist());

        // Wait a month until revisiting the page.
        AppBannerManager.setTimeDeltaForTesting(31);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(4));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(32);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(5));
        assertTrue(waitUntilNoInfoBarsExist());

        // Wait two months until revisiting the page, which should pop up the banner.
        AppBannerManager.setTimeDeltaForTesting(61);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(6));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(62);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(7));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedBannerDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(1));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
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
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(3));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(62);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(4));
        assertTrue(waitUntilNoInfoBarsExist());

        // Waiting three months should allow banners to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(5));
        assertTrue(waitUntilNoInfoBarsExist());

        AppBannerManager.setTimeDeltaForTesting(92);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));
        assertTrue(waitUntilAppDetailsRetrieved(6));
        assertTrue(waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE));
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBitmapFetchersCanOverlapWithoutCrashing() throws Exception {
        // Visit a site that requests a banner rapidly and repeatedly.
        for (int i = 1; i <= 10; i++) {
            assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                    new TabLoadObserver(getActivity().getActivityTab(), NATIVE_APP_URL)));

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
        // Visit the site in a new tab.
        loadUrlInNewTab("about:blank");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), WEB_APP_URL)));

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilNoInfoBarsExist());

        // Indicate a day has passed, then revisit the page to show the banner.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), WEB_APP_URL)));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilAppBannerInfoBarAppears(WEB_APP_TITLE));
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testWebAppSplashscreenIsDownloaded() throws Exception {
        // Sets the overriden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        // Visit the site in a new tab.
        loadUrlInNewTab("about:blank");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), WEB_APP_URL)));

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilNoInfoBarsExist());

        // Add the animation listener in.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);

        // Indicate a day has passed, then revisit the page to show the banner.
        AppBannerManager.setTimeDeltaForTesting(1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(
                new TabLoadObserver(getActivity().getActivityTab(), WEB_APP_URL)));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        }));
        assertTrue(waitUntilAppBannerInfoBarAppears(WEB_APP_TITLE));
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        }));

        // Click the button to trigger the adding of the shortcut.
        InfoBar infobar = container.getInfoBars().get(0);
        final Button button =
                (Button) infobar.getContentWrapper().findViewById(R.id.button_primary);
        TouchCommon.singleClickView(button);

        // Make sure that the splash screen icon was downloaded.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return dataStorageFactory.mSplashImage != null;
            }
        }));

        // Test that bitmap sizes match expectations.
        int idealSize = getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        assertEquals(idealSize, dataStorageFactory.mSplashImage.getWidth());
        assertEquals(idealSize, dataStorageFactory.mSplashImage.getHeight());
    }
}
