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
import android.os.Environment;
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
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.infobar.AppBannerInfoBarAndroid;
import org.chromium.chrome.browser.infobar.AppBannerInfoBarDelegateAndroid;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the app banners.
 */
public class AppBannerManagerTest extends ChromeTabbedActivityTestBase {
    private static final String NATIVE_APP_PATH =
            "/chrome/test/data/banners/play_app_test_page.html";

    private static final String NATIVE_ICON_PATH =
            "/chrome/test/data/banners/launcher-icon-4x.png";

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_PACKAGE = "123456";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String NATIVE_APP_URL_WITH_MANIFEST_PATH =
            "/chrome/test/data/banners/play_app_url_test_page.html";

    private static final String WEB_APP_PATH =
            "/chrome/test/data/banners/manifest_test_page.html";

    private static final String WEB_APP_TITLE = "Manifest test app";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private class MockAppDetailsDelegate extends AppDetailsDelegate {
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
            mAppData.setPackageInfo(NATIVE_APP_TITLE, mTestServer.getURL(NATIVE_ICON_PATH), 4.5f,
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

    private static class InfobarListener implements InfoBarAnimationListener {
        private boolean mDoneAnimating;

        @Override
        public void notifyAnimationFinished(int animationType) {
            if (animationType == InfoBarAnimationListener.ANIMATION_TYPE_SHOW) {
                mDoneAnimating = true;
            }
        }
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    private String mNativeAppUrl;
    private TestPackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;
    private String mWebAppUrl;

    public AppBannerManagerTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        mPackageManager = new TestPackageManager();
        AppBannerManager.setIsEnabledForTesting(true);
        AppBannerInfoBarDelegateAndroid.setPackageManagerForTesting(mPackageManager);
        ShortcutHelper.setDelegateForTests(new ShortcutHelper.Delegate() {
            @Override
            public void sendBroadcast(Context context, Intent intent) {
                // Ignore to prevent adding homescreen shortcuts.
            }
        });

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

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        mNativeAppUrl = mTestServer.getURL(NATIVE_APP_PATH);
        mWebAppUrl = mTestServer.getURL(WEB_APP_PATH);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    private void waitUntilNoInfoBarsExist() throws Exception {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInfoBars().isEmpty();
            }
        });
    }

    private void waitUntilAppDetailsRetrieved(final int numExpected) throws Exception {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return mDetailsDelegate.mNumRetrieved == numExpected
                        && !manager.isFetcherActiveForTesting();
            }
        });
    }

    private void waitUntilAppBannerInfoBarAppears(final String title) throws Exception {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infobars = getInfoBars();
                if (infobars.size() != 1) return false;
                if (!(infobars.get(0) instanceof AppBannerInfoBarAndroid)) return false;

                TextView textView = (TextView) infobars.get(0).getView().findViewById(
                        R.id.infobar_message);
                if (textView == null) return false;
                return TextUtils.equals(textView.getText(), title);
            }
        });
    }

    private void runFullNativeInstallPathway(String url, String expectedReferrer) throws Exception {
        // Visit a site that requests a banner.
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(url);
        waitUntilAppDetailsRetrieved(1);
        assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);
        waitUntilNoInfoBarsExist();

        // Indicate a day has passed, then revisit the page to get the banner to appear.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);
        AppBannerManager.setTimeDeltaForTesting(1);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(url);
        waitUntilAppDetailsRetrieved(2);
        waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        });

        // Check that the button asks if the user wants to install the app.
        InfoBar infobar = container.getInfoBarsForTesting().get(0);
        final Button button =
                (Button) infobar.getView().findViewById(R.id.button_primary);
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
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInstrumentation().checkMonitorHit(activityMonitor, 1)
                        && TextUtils.equals(button.getText(), installingText);
            }
        });

        // Say that the package is installed.  Infobar should say that the app is ready to open.
        mPackageManager.isInstalled = true;
        final String openText =
                getInstrumentation().getTargetContext().getString(R.string.app_banner_open);
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals(button.getText(), openText);
            }
        });
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testFullNativeInstallPathwayFromId() throws Exception {
        runFullNativeInstallPathway(mNativeAppUrl, NATIVE_APP_BLANK_REFERRER);
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testFullNativeInstallPathwayFromUrl() throws Exception {
        runFullNativeInstallPathway(
                mTestServer.getURL(NATIVE_APP_URL_WITH_MANIFEST_PATH), NATIVE_APP_REFERRER);
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBannerAppearsThenDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(1);
        waitUntilNoInfoBarsExist();

        // Indicate a day has passed, then revisit the page.
        AppBannerManager.setTimeDeltaForTesting(1);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(2);
        waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE);

        // Revisit the page to make the banner go away, but don't explicitly dismiss it.
        // This hides the banner for a few months.
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(3);
        waitUntilNoInfoBarsExist();

        // Wait a month until revisiting the page.
        AppBannerManager.setTimeDeltaForTesting(31);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(4);
        waitUntilNoInfoBarsExist();

        AppBannerManager.setTimeDeltaForTesting(32);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(5);
        waitUntilNoInfoBarsExist();

        // Wait two months until revisiting the page, which should pop up the banner.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(6);
        waitUntilNoInfoBarsExist();

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(7);
        waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE);
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedBannerDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(1);
        waitUntilNoInfoBarsExist();

        // Indicate a day has passed, then revisit the page.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);
        AppBannerManager.setTimeDeltaForTesting(1);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(2);
        waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE);

        // Explicitly dismiss the banner.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        });
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        waitUntilNoInfoBarsExist();

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(3);
        waitUntilNoInfoBarsExist();

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(4);
        waitUntilNoInfoBarsExist();

        // Waiting three months should allow banners to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(5);
        waitUntilNoInfoBarsExist();

        AppBannerManager.setTimeDeltaForTesting(92);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);
        waitUntilAppDetailsRetrieved(6);
        waitUntilAppBannerInfoBarAppears(NATIVE_APP_TITLE);
    }

    @MediumTest
    @Feature({"AppBanners"})
    public void testBitmapFetchersCanOverlapWithoutCrashing() throws Exception {
        // Visit a site that requests a banner rapidly and repeatedly.
        for (int i = 1; i <= 10; i++) {
            new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mNativeAppUrl);

            final Integer iteration = Integer.valueOf(i);
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mDetailsDelegate.mNumRetrieved == iteration;
                }
            });
        }
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testWebAppBannerAppears() throws Exception {
        // Visit the site in a new tab.
        loadUrlInNewTab("about:blank");
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mWebAppUrl);

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        });
        waitUntilNoInfoBarsExist();

        // Indicate a day has passed, then revisit the page to show the banner.
        AppBannerManager.setTimeDeltaForTesting(1);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mWebAppUrl);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        });
        waitUntilAppBannerInfoBarAppears(WEB_APP_TITLE);
    }

    @SmallTest
    @Feature({"AppBanners"})
    public void testWebAppSplashscreenIsDownloaded() throws Exception {
        // Sets the overriden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        // Visit the site in a new tab.
        loadUrlInNewTab("about:blank");
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mWebAppUrl);

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        });
        waitUntilNoInfoBarsExist();

        // Add the animation listener in.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.setAnimationListener(listener);

        // Indicate a day has passed, then revisit the page to show the banner.
        AppBannerManager.setTimeDeltaForTesting(1);
        new TabLoadObserver(getActivity().getActivityTab()).fullyLoadUrl(mWebAppUrl);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        getActivity().getActivityTab().getAppBannerManagerForTesting();
                return !manager.isFetcherActiveForTesting();
            }
        });
        waitUntilAppBannerInfoBarAppears(WEB_APP_TITLE);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return listener.mDoneAnimating;
            }
        });

        // Click the button to trigger the adding of the shortcut.
        InfoBar infobar = container.getInfoBarsForTesting().get(0);
        final Button button =
                (Button) infobar.getView().findViewById(R.id.button_primary);
        TouchCommon.singleClickView(button);

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return dataStorageFactory.mSplashImage != null;
            }
        });

        // Test that bitmap sizes match expectations.
        int idealSize = getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        assertEquals(idealSize, dataStorageFactory.mSplashImage.getWidth());
        assertEquals(idealSize, dataStorageFactory.mSplashImage.getHeight());
    }
}
