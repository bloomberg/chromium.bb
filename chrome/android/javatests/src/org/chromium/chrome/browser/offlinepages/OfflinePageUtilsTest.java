// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflinePageUtils}. */
@CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES})
public class OfflinePageUtilsTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TAG = "OfflinePageUtilsTest";
    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
    private static final int TIMEOUT_MS = 5000;
    private static final BookmarkId BOOKMARK_ID = new BookmarkId(1234, BookmarkType.NORMAL);

    private OfflinePageBridge mOfflinePageBridge;

    public OfflinePageUtilsTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure we start in an offline state.
                NetworkChangeNotifier.forceConnectivityState(false);

                Profile profile = Profile.getLastUsedProfile();
                mOfflinePageBridge = new OfflinePageBridge(profile);
                // Context context1 = getInstrumentation().getTargetContext();
                Context context2 = getActivity().getBaseContext();
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(context2);
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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    /**
     * Mock implementation of the SnackbarController.
     */
    static class MockSnackbarController implements SnackbarController {
        private int mButtonType;
        private boolean mDismissed;
        private static final long SNACKBAR_TIMEOUT = 7 * 1000;
        private static final long POLLING_INTERVAL = 100;

        public MockSnackbarController() {
            super();
            mButtonType = OfflinePageUtils.RELOAD_BUTTON;
            mDismissed = false;
        }

        public void waitForSnackbarControllerToFinish() {
            try {
                CriteriaHelper.pollForUIThreadCriteria(
                        new Criteria("Failed while waiting for snackbar calls to complete.") {
                            @Override
                            public boolean isSatisfied() {
                                return mDismissed;
                            }
                        },
                        SNACKBAR_TIMEOUT, POLLING_INTERVAL);
            } catch (InterruptedException e) {
                fail("Failed while waiting for snackbar calls to complete." + e);
            }
        }

        @Override
        public void onAction(Object actionData) {
            Log.d(TAG, "onAction for snackbar");
            mButtonType = (int) actionData;
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            Log.d(TAG, "onDismissNoAction for snackbar");
            if (actionData == null) return;
            mButtonType = (int) actionData;
            mDismissed = true;
        }

        public int getLastButtonType() {
            return mButtonType;
        }

        public boolean getDismissed() {
            return mDismissed;
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    public void testShowOfflineSnackbarIfNecessary() throws Exception {
        // Arrange - build a mock controller for sensing.
        Log.d(TAG, "Starting test");
        final MockSnackbarController mockSnackbarController = new MockSnackbarController();
        Log.d(TAG, "mockSnackbarController " + mockSnackbarController);

        // Save an offline page.
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);

        // Load an offline page into the current tab.  Note that this will create a
        // SnackbarController when the page loads, but we use our own for the test. The one created
        // here will also get the notification, but that won't interfere with our test.
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        String offlinePageUrl = offlinePage.getOfflineUrl();
        loadUrl(offlinePageUrl);
        Log.d(TAG, "Calling showOfflineSnackbarIfNecessary from test");

        // Act.  This needs to be called from the UI thread.
        Log.d(TAG, "before connecting NCN online state " + NetworkChangeNotifier.isOnline());
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Showing offline snackbar from UI thread");
                OfflinePageUtils.showOfflineSnackbarIfNecessary(
                        getActivity(), getActivity().getActivityTab(), mockSnackbarController);

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
        Log.d(TAG, "last button = " + mockSnackbarController.getLastButtonType());
        Log.d(TAG, "dismissed = " + mockSnackbarController.getDismissed());
        assertEquals(OfflinePageUtils.RELOAD_BUTTON, mockSnackbarController.getLastButtonType());
        assertTrue(mockSnackbarController.getDismissed());
    }

    // TODO(petewil): This is borrowed from OfflinePageBridge test.  We should refactor
    // to some common test code (including the setup).
    private void savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.savePage(getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(int savePageResult, String url) {
                                assertEquals(
                                        "Requested and returned URLs differ.", expectedUrl, url);
                                assertEquals(
                                        "Save result incorrect.", expectedResult, savePageResult);
                                semaphore.release();
                            }
                        });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private List<OfflinePageItem> getAllPages() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.clear();
                for (OfflinePageItem item : mOfflinePageBridge.getAllPages()) {
                    result.add(item);
                }

                semaphore.release();
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }
}
