// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for the Last 1 feature of Offline Pages. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class RecentTabsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

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
        assertAcquire(semaphore);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init();
                }
                NetworkChangeNotifier.forceConnectivityState(true);
            }
        });

        initializeBridgeForProfile(false);

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageSavedWhenTabSwitched() throws Exception {
        // The tab of interest.
        Tab tab = mActivityTestRule.loadUrlInNewTab(mTestPage);

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        Assert.assertNull(getPageByClientId(firstTabClientId));

        // Note: switching to a new tab must occur after the SnapshotController believes the page
        // quality is good enough.  With the debug flag, the delay after DomContentLoaded is 0 so we
        // can definitely snapshot after onload (which is what |loadUrlInNewTab| waits for).

        // Switch to a new tab to cause the WebContents hidden event.
        mActivityTestRule.loadUrlInNewTab("about:blank");

        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Note: this test relies on a sleeping period because some of the monitored actions are
     * difficult to track deterministically. A sleep time of 100 ms was chosen based on local
     * testing and is expected to be "safe". Nevertheless if flakiness is detected it might have to
     * be further increased.
     */
    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNClosingTabIsNotSaved() throws Exception {
        // Create the tab of interest.
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestPage);
        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        TabModelSelector tabModelSelector = tab.getTabModelSelector();
        Assert.assertEquals(tabModelSelector.getCurrentTab(), tab);
        Assert.assertFalse(tab.isHidden());
        Assert.assertNull(getPageByClientId(firstTabClientId));

        // The tab model is expected to support pending closures.
        final TabModel tabModel = tabModelSelector.getModelForTabId(tab.getId());
        Assert.assertTrue(tabModel.supportsPendingClosures());

        // Requests closing of the tab allowing for closure undo and checks it's actually closing.
        boolean closeTabReturnValue = ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return tabModel.closeTab(tab, false, false, true);
            }
        });
        Assert.assertTrue(closeTabReturnValue);
        Assert.assertTrue(tab.isHidden());
        Assert.assertTrue(tab.isClosing());

        // Wait a bit and checks that no snapshot was created.
        Thread.sleep(100); // Note: Flakiness potential here.
        Assert.assertNull(getPageByClientId(firstTabClientId));

        // Undo the closure and make sure the tab is again the current one on foreground.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tabModel.cancelTabClosure(tab.getId());
                int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                TabModelUtils.setIndex(tabModel, tabIndex);
            }
        });
        Assert.assertFalse(tab.isHidden());
        Assert.assertFalse(tab.isClosing());
        Assert.assertEquals(tabModelSelector.getCurrentTab(), tab);

        // Finally switch to a new tab and check that a snapshot is created.
        Tab newTab = mActivityTestRule.loadUrlInNewTab("about:blank");
        Assert.assertEquals(tabModelSelector.getCurrentTab(), newTab);
        Assert.assertTrue(tab.isHidden());
        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Verifies that a snapshot created by last_n is properly deleted when the tab is navigated to
     * another page. The deletion of snapshots for pages that should not be available anymore is a
     * privacy requirement for last_n.
     */
    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageIsDeletedUponNavigation() throws Exception {
        // The tab of interest.
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestPage);
        final TabModelSelector tabModelSelector = tab.getTabModelSelector();

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // Switch to a new tab and wait for the snapshot to be created.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        Assert.assertFalse(tab.equals(tabModelSelector.getCurrentTab()));
        OfflinePageItem offlinePage = waitForPageWithClientId(firstTabClientId);

        // Switch back to the initial tab.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel tabModel = tabModelSelector.getModelForTabId(tab.getId());
                int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                TabModelUtils.setIndex(tabModel, tabIndex);
            }
        });
        Assert.assertEquals(tabModelSelector.getCurrentTab(), tab);

        // Navigate to a new page and confirm that the previously created snapshot has been deleted.
        Semaphore deletionSemaphore = installPageDeletionSemaphore(offlinePage.getOfflineId());
        mActivityTestRule.loadUrl(mTestPage2);
        assertAcquire(deletionSemaphore);
    }

    /**
     * Verifies that a snapshot created by last_n is properly deleted when the tab is closed. The
     * deletion of snapshots for pages that should not be available anymore is a privacy requirement
     * for last_n.
     */
    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageIsDeletedUponClosure() throws Exception {
        // The tab of interest.
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestPage);
        final TabModelSelector tabModelSelector = tab.getTabModelSelector();

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // Switch to a new tab and wait for the snapshot to be created.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        OfflinePageItem offlinePage = waitForPageWithClientId(firstTabClientId);

        // Requests closing the tab allowing for undo -- so to exercise the subscribed notification
        // -- but immediately requests final closure.
        final int tabId = tab.getId();
        Semaphore deletionSemaphore = installPageDeletionSemaphore(offlinePage.getOfflineId());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel tabModel = tabModelSelector.getModelForTabId(tabId);
                tabModel.closeTab(tab, false, false, true);
                tabModel.commitTabClosure(tabId);
            }
        });

        // Checks that the tab is no more and that the snapshot was deleted.
        Assert.assertNull(tabModelSelector.getTabById(tabId));
        assertAcquire(deletionSemaphore);
    }

    private void assertAcquire(Semaphore semaphore) throws InterruptedException {
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private OfflinePageItem waitForPageWithClientId(final ClientId clientId)
            throws InterruptedException {
        OfflinePageItem item = getPageByClientId(clientId);
        if (item != null) return item;

        final Semaphore semaphore = new Semaphore(0);
        final AtomicReference<OfflinePageItem> itemReference =
                new AtomicReference<OfflinePageItem>();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageAdded(OfflinePageItem newPage) {
                        if (newPage.getClientId().equals(clientId)) {
                            itemReference.set(newPage);
                            mOfflinePageBridge.removeObserver(this);
                            semaphore.release();
                        }
                    }
                });
            }
        });
        assertAcquire(semaphore);
        return itemReference.get();
    }

    private Semaphore installPageDeletionSemaphore(final long offlineId) {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageDeleted(DeletedPageInfo deletedPage) {
                        if (deletedPage.getOfflineId() == offlineId) {
                            mOfflinePageBridge.removeObserver(this);
                            semaphore.release();
                        }
                    }
                });
            }
        });
        return semaphore;
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
        assertAcquire(semaphore);
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
        assertAcquire(semaphore);
        return result[0];
    }
}
