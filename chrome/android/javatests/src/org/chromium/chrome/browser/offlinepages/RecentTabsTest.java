// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.support.annotation.MainThread;
import android.support.test.filters.MediumTest;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Integration tests for the Last 1 feature of Offline Pages. */
@CommandLineFlags.Add("enable-features=OfflineRecentPages")
public class RecentTabsTest extends ChromeTabbedActivityTestBase {
    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/simple.html";
    private static final int TIMEOUT_MS = 5000;

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private String mTestPage2;

    private void initializeBridgeForProfile(final boolean incognitoProfile)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                if (incognitoProfile) {
                    profile = profile.getOffTheRecordProfile();
                }
                // Ensure we start in an offline state.
                mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
                if (mOfflinePageBridge == null || mOfflinePageBridge.isOfflinePageModelLoaded()) {
                    semaphore.release();
                    return;
                }
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageModelLoaded() {
                        semaphore.release();
                        mOfflinePageBridge.removeObserver(this);
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure we start in an offline state.
                NetworkChangeNotifier.forceConnectivityState(false);
                Context context = getActivity().getBaseContext();
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(context);
                }
            }
        });

        initializeBridgeForProfile(false);

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageSavedWhenTabSwitched() throws Exception {
        // The tab of interest.
        Tab tab = loadUrlInNewTab(mTestPage);

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        assertNull(getPageByClientId(firstTabClientId));

        // Note: switching to a new tab must occur after the SnapshotController believes the page
        // quality is good enough.  With the debug flag, the delay after DomContentLoaded is 0 so we
        // can definitely snapshot after onload (which is what |loadUrlInNewTab| waits for).

        // Switch to a new tab to cause the WebContents hidden event.
        loadUrlInNewTab("about:blank");

        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Note: this test relies on a sleeping period because some of the monitored actions are
     * difficult to track deterministically. A sleep time of 100 ms was chosen based on local
     * testing and is expected to be "safe". Nevertheless if flakiness is detected it might have to
     * be further increased.
     */
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNClosingTabIsNotSaved() throws Exception {
        // Create the tab of interest.
        final Tab tab = loadUrlInNewTab(mTestPage);
        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        TabModelSelector tabModelSelector = tab.getTabModelSelector();
        assertEquals(tabModelSelector.getCurrentTab(), tab);
        assertFalse(tab.isHidden());
        assertNull(getPageByClientId(firstTabClientId));

        // The tab model is expected to support pending closures.
        final TabModel tabModel = tabModelSelector.getModelForTabId(tab.getId());
        assertTrue(tabModel.supportsPendingClosures());

        // Requests closing of the tab allowing for closure undo and checks it's actually closing.
        boolean closeTabReturnValue = ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return tabModel.closeTab(tab, false, false, true);
            }
        });
        assertTrue(closeTabReturnValue);
        assertTrue(tab.isHidden());
        assertTrue(tab.isClosing());

        // Wait a bit and checks that no snapshot was created.
        Thread.sleep(100); // Note: Flakiness potential here.
        assertNull(getPageByClientId(firstTabClientId));

        // Undo the closure and make sure the tab is again the current one on foreground.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tabModel.cancelTabClosure(tab.getId());
                int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                TabModelUtils.setIndex(tabModel, tabIndex);
            }
        });
        assertFalse(tab.isHidden());
        assertFalse(tab.isClosing());
        assertEquals(tabModelSelector.getCurrentTab(), tab);

        // Finally switch to a new tab and check that a snapshot is created.
        Tab newTab = loadUrlInNewTab("about:blank");
        assertEquals(tabModelSelector.getCurrentTab(), newTab);
        assertTrue(tab.isHidden());
        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Verifies that a snapshot created by last_n is properly deleted when the tab is navigated to
     * another page. The deletion of snapshots for pages that should not be available anymore is a
     * privacy requirement for last_n.
     */
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageIsDeletedUponNavigation() throws Exception {
        // The tab of interest.
        final Tab tab = loadUrlInNewTab(mTestPage);
        final TabModelSelector tabModelSelector = tab.getTabModelSelector();

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // Switch to a new tab and wait for the snapshot to be created.
        loadUrlInNewTab("about:blank");
        waitForPageWithClientId(firstTabClientId);
        OfflinePageItem offlinePage = getPageByClientId(firstTabClientId);
        assertFalse(tab.equals(tabModelSelector.getCurrentTab()));

        // Switch back to the initial tab and install the page deletion monitor for later usage.
        final OfflinePageDeletionMonitor deletionMonitor =
                new OfflinePageDeletionMonitor(offlinePage.getOfflineId());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel tabModel = tabModelSelector.getModelForTabId(tab.getId());
                int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                TabModelUtils.setIndex(tabModel, tabIndex);
                deletionMonitor.installObserver();
            }
        });
        assertEquals(tabModelSelector.getCurrentTab(), tab);

        // Navigate to a new page and confirm the previously created snapshot has been deleted.
        loadUrl(mTestPage2);
        deletionMonitor.assertDeleted();
    }

    /**
     * Verifies that a snapshot created by last_n is properly deleted when the tab is closed. The
     * deletion of snapshots for pages that should not be available anymore is a privacy requirement
     * for last_n.
     */
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageIsDeletedUponClosure() throws Exception {
        // The tab of interest.
        final Tab tab = loadUrlInNewTab(mTestPage);
        final TabModelSelector tabModelSelector = tab.getTabModelSelector();

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // Switch to a new tab and wait for the snapshot to be created.
        loadUrlInNewTab("about:blank");
        waitForPageWithClientId(firstTabClientId);
        OfflinePageItem offlinePage = getPageByClientId(firstTabClientId);

        // Requests closing of the tab allowing for closure undo and checks it's actually closing.
        final int tabId = tab.getId();
        final OfflinePageDeletionMonitor deletionMonitor =
                new OfflinePageDeletionMonitor(offlinePage.getOfflineId());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                deletionMonitor.installObserver();
                TabModel tabModel = tabModelSelector.getModelForTabId(tabId);
                tabModel.closeTab(tab, false, false, true);
                tabModel.commitTabClosure(tabId);
            }
        });
        assertNull(tabModelSelector.getTabById(tabId));
        deletionMonitor.assertDeleted();
    }

    private void waitForPageWithClientId(final ClientId clientId) throws InterruptedException {
        if (getPageByClientId(clientId) != null) return;

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageAdded(OfflinePageItem newPage) {
                        if (newPage.getClientId().equals(clientId)) {
                            mOfflinePageBridge.removeObserver(this);
                            semaphore.release();
                        }
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private class OfflinePageDeletionMonitor {
        private final Semaphore mSemaphore = new Semaphore(0);
        private final long mOfflineId;

        public OfflinePageDeletionMonitor(long offlineId) {
            mOfflineId = offlineId;
        }

        @MainThread
        public void installObserver() {
            mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                @Override
                public void offlinePageDeleted(long offlineId, ClientId clientId) {
                    if (offlineId == mOfflineId) {
                        mOfflinePageBridge.removeObserver(this);
                        mSemaphore.release();
                    }
                }
            });
        }

        public void assertDeleted() throws InterruptedException {
            assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        }
    }

    private OfflinePageItem getPageByClientId(ClientId clientId) throws InterruptedException {
        final OfflinePageItem[] result = {null};
        final Semaphore semaphore = new Semaphore(0);
        final List<ClientId> clientIdList = new ArrayList<>();
        clientIdList.add(clientId);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getPagesByClientIds(
                        clientIdList, new Callback<List<OfflinePageItem>>() {
                            @Override
                            public void onResult(List<OfflinePageItem> items) {
                                if (!items.isEmpty()) {
                                    result[0] = items.get(0);
                                }
                                semaphore.release();
                            }
                        });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result[0];
    }

    private OfflinePageItem getPageByOfflineId(final long offlineId) throws InterruptedException {
        final OfflinePageItem[] result = {null};
        final Semaphore semaphore = new Semaphore(0);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getPageByOfflineId(offlineId, new Callback<OfflinePageItem>() {
                    @Override
                    public void onResult(OfflinePageItem item) {
                        result[0] = item;
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result[0];
    }
}
