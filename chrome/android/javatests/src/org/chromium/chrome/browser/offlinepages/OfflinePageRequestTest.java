// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline page request handling. */
@CommandLineFlags.Add("enable-features=OfflineBookmarks")
public class OfflinePageRequestTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final String ABOUT_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId CLIENT_ID =
            new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;

    public OfflinePageRequestTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(getActivity().getBaseContext());
                }
                NetworkChangeNotifier.forceConnectivityState(true);

                Profile profile = Profile.getLastUsedProfile();
                mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
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

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @RetryOnFailure
    public void testLoadOfflinePageOnDisconnectedNetwork() throws Exception {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                getInstrumentation().getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        String aboutUrl = testServer.getURL(ABOUT_PAGE);

        Tab tab = getActivity().getActivityTab();

        // Load and save an offline page.
        savePage(testUrl);
        assertFalse(isErrorPage(tab));
        assertFalse(isOfflinePage(tab));

        // Load another page.
        loadUrl(aboutUrl);
        assertFalse(isErrorPage(tab));
        assertFalse(isOfflinePage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.forceConnectivityState(false);
            }
        });

        // Load the page that has an offline copy. The offline page should be shown.
        loadUrl(testUrl);
        assertFalse(isErrorPage(tab));
        assertTrue(isOfflinePage(tab));
    }

    @SmallTest
    @RetryOnFailure
    public void testLoadOfflinePageWithFragmentOnDisconnectedNetwork() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        String testUrlWithFragment = testUrl + "#ref";

        Tab tab = getActivity().getActivityTab();

        // Load and save an offline page for the url with a fragment.
        savePage(testUrlWithFragment);
        assertFalse(isErrorPage(tab));
        assertFalse(isOfflinePage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.forceConnectivityState(false);
            }
        });

        // Load the URL without the fragment. The offline page should be shown.
        loadUrl(testUrl);
        assertFalse(isErrorPage(tab));
        assertTrue(isOfflinePage(tab));
    }

    private void savePage(String url) throws InterruptedException {
        loadUrl(url);

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.savePage(getActivity().getActivityTab().getWebContents(),
                        CLIENT_ID, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {
                                assertEquals(
                                        "Save failed.", SavePageResult.SUCCESS, savePageResult);
                                semaphore.release();
                            }
                        });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private boolean isOfflinePage(final Tab tab) {
        final boolean[] isOffline = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isOffline[0] = tab.isOfflinePage();
            }
        });
        return isOffline[0];
    }

    private boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isShowingError[0] = tab.isShowingErrorPage();
            }
        });
        return isShowingError[0];
    }
}
