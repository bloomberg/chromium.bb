// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
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
    private static final int TIMEOUT_MS = 5000;

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;

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

        // Note, that switching to a new tab must occur after the SnapshotController believes the
        // page quality is good enough.  With the debug flag, the delay after DomContentLoaded is 0
        // so we can definitely snapshot after onload (which is what |loadUrlInNewTab| waits for).

        // Switch to a new tab to cause the WebContents hidden event.
        loadUrlInNewTab("about:blank");

        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Note: this test relies on a sleeping period because some of the taking actions are
     * complicated to track otherwise, so there is the possibility of flakiness. I chose 100ms from
     * local testing and I expect it to be "safe" but it flakiness is detected it might have to be
     * further increased.
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
}
