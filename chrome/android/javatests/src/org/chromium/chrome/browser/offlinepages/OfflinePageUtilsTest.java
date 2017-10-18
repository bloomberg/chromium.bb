// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Instrumentation tests for {@link OfflinePageUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-features=OfflinePagesSharing",
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class OfflinePageUtilsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId BOOKMARK_ID =
            new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure we start in an online state.
                NetworkChangeNotifier.forceConnectivityState(true);

                Profile profile = Profile.getLastUsedProfile();
                mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init();
                }
                if (mOfflinePageBridge.isOfflinePageModelLoaded()) {
                    semaphore.release();
                } else {
                    mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                        @Override
                        public void offlinePageModelLoaded() {
                            semaphore.release();
                            mOfflinePageBridge.removeObserver(this);
                        }
                    });
                }
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Mock implementation of the SnackbarController.
     */
    static class MockSnackbarController implements SnackbarController {
        private int mTabId;
        private boolean mDismissed;
        private static final long SNACKBAR_TIMEOUT = 7 * 1000;
        private static final long POLLING_INTERVAL = 100;

        public MockSnackbarController() {
            super();
            mTabId = Tab.INVALID_TAB_ID;
            mDismissed = false;
        }

        public void waitForSnackbarControllerToFinish() {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed while waiting for snackbar calls to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return mDismissed;
                        }
                    },
                    SNACKBAR_TIMEOUT, POLLING_INTERVAL);
        }

        @Override
        public void onAction(Object actionData) {
            mTabId = (int) actionData;
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            if (actionData == null) return;
            mTabId = (int) actionData;
            mDismissed = true;
        }

        public int getLastTabId() {
            return mTabId;
        }

        public boolean getDismissed() {
            return mDismissed;
        }
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testShowOfflineSnackbarIfNecessary() throws Exception {
        // Arrange - build a mock controller for sensing.
        OfflinePageUtils.setSnackbarDurationForTesting(1000);
        final MockSnackbarController mockSnackbarController = new MockSnackbarController();

        // Save an offline page.
        loadPageAndSave();

        // With network disconnected, loading an online URL will result in loading an offline page.
        // Note that this will create a SnackbarController when the page loads, but we use our own
        // for the test. The one created here will also get the notification, but that won't
        // interfere with our test.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.forceConnectivityState(false);
            }
        });
        String testUrl = mTestServer.getURL(TEST_PAGE);
        mActivityTestRule.loadUrl(testUrl);

        int tabId = mActivityTestRule.getActivity().getActivityTab().getId();

        // Act.  This needs to be called from the UI thread.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                OfflinePageTabObserver offlineObserver = new OfflinePageTabObserver(
                        mActivityTestRule.getActivity().getTabModelSelector(),
                        mActivityTestRule.getActivity().getSnackbarManager(),
                        mockSnackbarController);
                OfflinePageTabObserver.setObserverForTesting(
                        mActivityTestRule.getActivity(), offlineObserver);
                OfflinePageUtils.showOfflineSnackbarIfNecessary(
                        mActivityTestRule.getActivity().getActivityTab());

                // Pretend that we went online, this should cause the snackbar to show.
                // This call will set the isConnected call to return true.
                NetworkChangeNotifier.forceConnectivityState(true);
                // This call will make an event get sent with connection type CONNECTION_WIFI.
                NetworkChangeNotifier.fakeNetworkConnected(0, ConnectionType.CONNECTION_WIFI);
            }
        });

        // Wait for the snackbar to be dismissed before we check its values.  The snackbar is on a
        // three second timer, and will dismiss itself in about 3 seconds.
        mockSnackbarController.waitForSnackbarControllerToFinish();

        // Assert snackbar was shown.
        Assert.assertEquals(tabId, mockSnackbarController.getLastTabId());
        Assert.assertTrue(mockSnackbarController.getDismissed());
    }

    private void loadPageAndSave() throws Exception {
        String testUrl = mTestServer.getURL(TEST_PAGE);
        mActivityTestRule.loadUrl(testUrl);
        savePage(SavePageResult.SUCCESS, testUrl);
    }

    // TODO(petewil): This is borrowed from OfflinePageBridge test.  We should refactor
    // to some common test code (including the setup).  crbug.com/705100.
    private void savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.savePage(
                        mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {
                                Assert.assertEquals(
                                        "Requested and returned URLs differ.", expectedUrl, url);
                                Assert.assertEquals(
                                        "Save result incorrect.", expectedResult, savePageResult);
                                semaphore.release();
                            }
                        });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private List<OfflinePageItem> getAllPages() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getAllPages(new Callback<List<OfflinePageItem>>() {
                    @Override
                    public void onResult(List<OfflinePageItem> allPages) {
                        result.addAll(allPages);
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testCopyToShareableLocation() throws Exception {
        // Save an offline page.
        loadPageAndSave();

        // Get an offline page from the list and obtain the file path.
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        String offlinePageFilePath = offlinePage.getFilePath();

        File offlinePageOriginal = new File(offlinePageFilePath);

        // Clear the directory before copying the file.
        clearSharedOfflineFilesAndWait();

        File offlineSharingDir = OfflinePageUtils.getDirectoryForOfflineSharing(
                mActivityTestRule.getActivity().getBaseContext());
        Assert.assertTrue("Offline sharing directory should exist.", offlineSharingDir != null);

        File offlinePageShareable = new File(offlineSharingDir, offlinePageOriginal.getName());
        Assert.assertFalse(
                "File with the same name should not exist.", offlinePageShareable.exists());

        Assert.assertTrue("Should be able to copy file to shareable location.",
                OfflinePageUtils.copyToShareableLocation(
                        offlinePageOriginal, offlinePageShareable));

        Assert.assertEquals("File copy result incorrect", offlinePageOriginal.length(),
                offlinePageShareable.length());
    }

    @Test
    @SmallTest
    public void testDeleteSharedOfflineFiles() throws Exception {
        // Save an offline page.
        loadPageAndSave();

        // Copies file to external cache directory.
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        String offlinePageFilePath = offlinePage.getFilePath();

        File offlinePageOriginal = new File(offlinePageFilePath);

        final Context context = mActivityTestRule.getActivity().getBaseContext();
        final File offlineSharingDir = OfflinePageUtils.getDirectoryForOfflineSharing(context);

        Assert.assertTrue("Should be able to create subdirectory in shareable directory.",
                (offlineSharingDir != null));

        File offlinePageShareable = new File(offlineSharingDir, offlinePageOriginal.getName());
        if (!offlinePageShareable.exists()) {
            Assert.assertTrue("Should be able to copy file to shareable location.",
                    OfflinePageUtils.copyToShareableLocation(
                            offlinePageOriginal, offlinePageShareable));
        }

        // Clear files.
        clearSharedOfflineFilesAndWait();
    }

    private void clearSharedOfflineFilesAndWait() {
        final Context context = mActivityTestRule.getActivity().getBaseContext();
        final File offlineSharingDir = OfflinePageUtils.getDirectoryForOfflineSharing(context);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                OfflinePageUtils.clearSharedOfflineFiles(context);
            }
        });

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed while waiting for file operation to complete.") {
                    @Override
                    public boolean isSatisfied() {
                        return !offlineSharingDir.exists();
                    }
                });
    }
}
