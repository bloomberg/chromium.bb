// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests org.chromium.chrome.browser.webapps.AddToHomescreenManager and its C++ counterpart.
 */
@RetryOnFailure
public class AddToHomescreenManagerTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String WEBAPP_ACTION_NAME = "WEBAPP_ACTION";

    private static final String WEBAPP_TITLE = "Webapp shortcut";
    private static final String WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");
    private static final String EDITED_WEBAPP_TITLE = "Webapp shortcut edited";

    private static final String SECOND_WEBAPP_TITLE = "Webapp shortcut #2";
    private static final String SECOND_WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + SECOND_WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable again</body></html>");

    private static final String NORMAL_TITLE = "Plain shortcut";
    private static final String NORMAL_HTML = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head><title>" + NORMAL_TITLE + "</title></head>"
            + "<body>Not Webapp capable</body></html>");

    private static final String META_APP_NAME_PAGE_TITLE = "Not the right title";
    private static final String META_APP_NAME_TITLE = "Web application-name";
    private static final String META_APP_NAME_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<meta name=\"application-name\" content=\"" + META_APP_NAME_TITLE + "\">"
            + "<title>" + META_APP_NAME_PAGE_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");

    private static final String MANIFEST_PATH = "/chrome/test/data/webapps/manifest_test_page.html";
    private static final String MANIFEST_TITLE = "Web app banner test page";

    private static final String MANIFEST_TIMES_OUT_NO_SERVICE_WORKER_HTML =
            UrlUtils.encodeHtmlDataUri("<html><head>"
                    + "<title>" + MANIFEST_TITLE + "</title>"
                    + "<link rel=\"manifest\" href=\"../../../../slow?10000\" />"
                    + "</head></html>");

    private static class TestShortcutHelperDelegate extends ShortcutHelper.Delegate {
        public Intent mBroadcastedIntent;

        @Override
        public void sendBroadcast(Context context, Intent intent) {
            mBroadcastedIntent = intent;
        }

        @Override
        public String getFullscreenAction() {
            return WEBAPP_ACTION_NAME;
        }

        public void clearBroadcastedIntent() {
            mBroadcastedIntent = null;
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

    /**
     * Test AddToHomescreenManager subclass which tracks whether the native callbacks
     * (a proxy for the AddToHomescreenDataFetcher::Observer callbacks) have been called.
     */
    private static class AddToHomescreenManagerCallbackTracker extends AddToHomescreenManager {
        public CallbackHelper mCallbackHelper;
        public boolean mDialogShown = false;
        public boolean mGotUserTitle = false;
        public boolean mReadyToAdd = false;

        public AddToHomescreenManagerCallbackTracker(Activity activity, Tab tab) {
            super(activity, tab);

            mCallbackHelper = new CallbackHelper();

            AddToHomescreenManager.Observer observer = new AddToHomescreenManager.Observer() {
                @Override
                public void onUserTitleAvailable(String title) {
                    mGotUserTitle = true;
                    mCallbackHelper.notifyCalled();
                }

                @Override
                public void onReadyToAdd(Bitmap icon) {
                    mReadyToAdd = true;
                    mCallbackHelper.notifyCalled();
                }
            };
            setObserver(observer);
        }

        public void waitForCallbacks(int numCallbacks, int numSecondsTimeout) throws Exception {
            mCallbackHelper.waitForCallback(0, numCallbacks, numSecondsTimeout, TimeUnit.SECONDS);
        }

        @Override
        public void showDialog() {
            mDialogShown = true;
            mCallbackHelper.notifyCalled();
        }
    }

    /**
     * Test AddToHomescreenManager subclass which mocks showing the add-to-homescreen dialog and
     * adds the shortcut to the home screen once it is ready.
     */
    private static class TestAddToHomescreenManager extends AddToHomescreenManager {
        private String mTitle;

        /**
         * Creates an instance of {@link TestAddToHomescreenManager}.
         * @param title The title that the user entered into the add-to-homescreen dialog.
         */
        public TestAddToHomescreenManager(Activity activity, Tab tab, String title) {
            super(activity, tab);
            mTitle = title;
        }

        @Override
        public void showDialog() {
            AddToHomescreenManager.Observer observer = new AddToHomescreenManager.Observer() {
                @Override
                public void onUserTitleAvailable(String title) {}

                @Override
                public void onReadyToAdd(Bitmap icon) {
                    TestAddToHomescreenManager.this.addShortcut(mTitle);
                }
            };

            setObserver(observer);
        }
    }

    private EmbeddedTestServer mTestServer;
    private ChromeActivity mActivity;
    private Tab mTab;
    private TestShortcutHelperDelegate mShortcutHelperDelegate;

    public AddToHomescreenManagerTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        // Register handler for "slow?10000" URL.
        mTestServer.addDefaultHandlers(mTestServer.getURL("/chrome/test/data"));
        mShortcutHelperDelegate = new TestShortcutHelperDelegate();
        ShortcutHelper.setDelegateForTests(mShortcutHelperDelegate);
        mActivity = getActivity();
        mTab = mActivity.getActivityTab();
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcuts() throws Exception {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, "");
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, launchIntent.getAction());
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mShortcutHelperDelegate.clearBroadcastedIntent();
        loadUrl(SECOND_WEBAPP_HTML, SECOND_WEBAPP_TITLE);
        addShortcutToTab(mTab, "");
        Intent newFiredIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(SECOND_WEBAPP_TITLE,
                newFiredIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent newLaunchIntent = newFiredIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(SECOND_WEBAPP_HTML, newLaunchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, newLaunchIntent.getAction());
        assertEquals(mActivity.getPackageName(), newLaunchIntent.getPackage());
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddBookmarkShortcut() throws Exception {
        loadUrl(NORMAL_HTML, NORMAL_TITLE);
        addShortcutToTab(mTab, "");

        // Make sure the intent's parameters make sense.
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(NORMAL_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());
        assertEquals(Intent.ACTION_VIEW, launchIntent.getAction());
        assertEquals(NORMAL_HTML, launchIntent.getDataString());
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithoutTitleEdit() throws Exception {
        // Add a webapp shortcut using the page's title.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, "");
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithTitleEdit() throws Exception {
        // Add a webapp shortcut with a custom title.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, EDITED_WEBAPP_TITLE);
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(EDITED_WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithApplicationName() throws Exception {
        loadUrl(META_APP_NAME_HTML, META_APP_NAME_PAGE_TITLE);
        addShortcutToTab(mTab, "");
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(META_APP_NAME_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContentSwitches.DISABLE_POPUP_BLOCKING)
    public void testAddWebappShortcutWithEmptyPage() throws Exception {
        Tab spawnedPopup = spawnPopupInBackground("");
        addShortcutToTab(spawnedPopup, "");
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutSplashScreenIcon() throws Exception {
        try {
            // Sets the overriden factory to observer splash screen update.
            final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
            WebappDataStorage.setFactoryForTests(dataStorageFactory);

            loadUrl(mTestServer.getURL(MANIFEST_PATH), MANIFEST_TITLE);
            addShortcutToTab(mTab, "");

            // Make sure that the splash screen image was downloaded.
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return dataStorageFactory.mSplashImage != null;
                }
            });

            // Test that bitmap sizes match expectations.
            int idealSize = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.webapp_splash_image_size_ideal);
            assertEquals(idealSize, dataStorageFactory.mSplashImage.getWidth());
            assertEquals(idealSize, dataStorageFactory.mSplashImage.getHeight());
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @MediumTest
    @Feature("{Webapp}")
    public void testAddShortcutManifestFetchTimesOutWhenWebApkEnabled() throws Exception {
        ChromeWebApkHost.initForTesting(true);
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, mTestServer.getURL("/slow?10000"));
        checkCallbacksCalledWhenAddShortcutManifestFetchTimesOut();
    }

    @MediumTest
    @Feature("{Webapp}")
    public void testAddShortcutManifestFetchTimesOutWhenWebApkDisabled() throws Exception {
        ChromeWebApkHost.initForTesting(false);
        WebappTestPage.navigateToPageWithServiceWorkerAndManifest(
                mTestServer, mTab, mTestServer.getURL("/slow?10000"));
        checkCallbacksCalledWhenAddShortcutManifestFetchTimesOut();
    }

    @MediumTest
    @Feature("{Webapp}")
    public void testAddShortcutManifestFetchTimesOutWhenNotPwa() throws Exception {
        ChromeWebApkHost.initForTesting(false);
        loadUrl(MANIFEST_TIMES_OUT_NO_SERVICE_WORKER_HTML, MANIFEST_TITLE);
        checkCallbacksCalledWhenAddShortcutManifestFetchTimesOut();
    }

    /**
     * Checks that the add-to-homescreen native callbacks are called when the Web Manifest fetch
     * times out.
     */
    public void checkCallbacksCalledWhenAddShortcutManifestFetchTimesOut() throws Exception {
        AddToHomescreenManagerCallbackTracker manager =
                new AddToHomescreenManagerCallbackTracker(mActivity, mTab);
        startManagerOnUiThread(manager);

        // The AddToHomescreenDataFetcher timeout fires after 4 seconds. Wait 10 seconds to ensure
        // that we do not miss the AddToHomescreenDataFetcher timeout timer firing.
        manager.waitForCallbacks(3, 10);

        assertTrue(manager.mDialogShown);
        // This callback enables the text field in the add-to-homescreen dialog.
        assertTrue(manager.mGotUserTitle);
        // This callback enables the "Add" button in the add-to-homescreen dialog.
        assertTrue(manager.mReadyToAdd);

        destroyManagerOnUiThread(manager);
    }

    private void loadUrl(String url, String expectedPageTitle) throws Exception {
        new TabLoadObserver(mTab, expectedPageTitle, null).fullyLoadUrl(url);
    }

    private void addShortcutToTab(Tab tab, String title) throws Exception {
        // Add the shortcut.
        TestAddToHomescreenManager manager = new TestAddToHomescreenManager(mActivity, tab, title);
        startManagerOnUiThread(manager);

        // Make sure that the shortcut was added.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mShortcutHelperDelegate.mBroadcastedIntent != null;
            }
        });

        destroyManagerOnUiThread(manager);
    }

    private void startManagerOnUiThread(final AddToHomescreenManager manager) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                manager.start();
            }
        });
    }

    private void destroyManagerOnUiThread(final AddToHomescreenManager manager) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                manager.destroy();
            }
        });
    }

    /**
     * Spawns popup via window.open() at {@link url}.
     */
    private Tab spawnPopupInBackground(final String url) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTab.getWebContents().evaluateJavaScriptForTests(
                        "(function() {"
                        + "window.open('" + url + "');"
                        + "})()",
                        null);
            }
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getActivity().getTabModelSelector().getModel(false).getCount();
            }
        }));

        TabModel tabModel = getActivity().getTabModelSelector().getModel(false);
        assertEquals(0, tabModel.indexOf(mTab));
        return getActivity().getTabModelSelector().getModel(false).getTabAt(1);
    }
}
