// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for OfflinePageUtils.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowMultiDex.class})
public class OfflinePageTabObserverTest {
    // Using a null tab, as it cannot be mocked. TabHelper will help return proper mocked responses.
    private static final int TAB_ID = 77;

    @Mock private Context mContext;
    @Mock
    private TabModel mTabModel;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SnackbarController mSnackbarController;
    @Mock private Tab mTab;

    private OfflinePageTabObserver createObserver() {
        OfflinePageTabObserver observer = spy(new OfflinePageTabObserver(
                mContext, mTabModel, mSnackbarManager, mSnackbarController));
        // Mocking out all of the calls that touch on NetworkChangeNotifier, which we cannot
        // directly mock out.
        doNothing().when(observer).startObservingNetworkChanges();
        doNothing().when(observer).stopObservingNetworkChanges();
        // TODO(fgorski): This call has to be mocked out until we update OfflinePageUtils.
        // It also goes to NetworkChangeNotifier from there.
        doReturn(false).when(observer).isConnected();
        doReturn(false).when(observer).isShowingOfflinePreview(any(Tab.class));
        // TODO(fgorski): This call has to be mocked out until we update OfflinePageUtils.
        doNothing().when(observer).showReloadSnackbar(any(Tab.class));
        // Assert tab model observer was created.
        assertTrue(observer.getTabModelObserver() != null);
        return observer;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        // Setting up a mock tab. These are the values common to most tests, but individual
        // tests might easily overwrite them.
        doReturn(TAB_ID).when(mTab).getId();
        doReturn(false).when(mTab).isFrozen();
        doReturn(false).when(mTab).isHidden();
        doReturn(true).when(mTab).isOfflinePage();

        // Setting up mock snackbar manager.
        doNothing().when(mSnackbarManager).dismissSnackbars(eq(mSnackbarController));
    }

    private void showTab(OfflinePageTabObserver observer) {
        doReturn(false).when(mTab).isHidden();
        if (observer != null) {
            observer.onShown(mTab);
        }
    }

    private void hideTab(OfflinePageTabObserver observer) {
        doReturn(true).when(mTab).isHidden();
        if (observer != null) {
            observer.onHidden(mTab);
        }
    }

    private void removeTab(OfflinePageTabObserver observer) {
        if (observer != null && observer.getTabModelObserver() != null) {
            observer.getTabModelObserver().tabRemoved(mTab);
        }
    }

    private void connect(OfflinePageTabObserver observer, boolean notify) {
        doReturn(true).when(observer).isConnected();
        if (notify) {
            observer.onConnectionTypeChanged(0);
        }
    }

    private void disconnect(OfflinePageTabObserver observer, boolean notify) {
        doReturn(false).when(observer).isConnected();
        if (notify) {
            observer.onConnectionTypeChanged(0);
        }
    }

    @Test
    @Feature({"OfflinePages"})
    public void testBasicState() {
        OfflinePageTabObserver observer = createObserver();
        assertFalse(observer.isObservingNetworkChanges());
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(false).when(mTab).isOfflinePage();
        observer.startObservingTab(mTab);

        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));

        doReturn(true).when(mTab).isOfflinePage();
        observer.startObservingTab(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStopObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);

        // Try to stop observing a tab that is not observed.
        doReturn(42).when(mTab).getId();
        observer.stopObservingTab(mTab);

        assertFalse(observer.isObservingTab(mTab));
        doReturn(TAB_ID).when(mTab).getId();

        observer.stopObservingTab(mTab);
        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        doReturn(true).when(observer).isConnected();
        observer.onPageLoadFinished(mTab);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onPageLoadFinished() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        observer.onPageLoadFinished(mTab);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        observer.onUrlUpdated(mTab);
        assertFalse(observer.isLoadedTab(mTab));

        observer.onPageLoadFinished(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished_hidden() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        observer.onUrlUpdated(mTab);
        assertFalse(observer.isLoadedTab(mTab));

        observer.onPageLoadFinished(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        showTab(observer);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDontShowPreviewSnackbar_onShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        doReturn(true).when(observer).isShowingOfflinePreview(mTab);
        observer.onPageLoadFinished(mTab);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        showTab(observer);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        observer.onShown(mTab);
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown_pageNotLoaded() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);

        observer.onShown(mTab);
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_afterSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        // Snackbar is showing over here.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));

        hideTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_beforeSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // Snackbar is not showing over here.
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));

        hideTab(observer);

        // Snackbars if any where dismissed regardless.
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnDestroyed() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        // Snackbar was shown, so all other conditions are met.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        observer.onDestroyed(mTab);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mTab, times(1)).removeObserver(eq(observer));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated_whenNotShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mTab).isOfflinePage();
        observer.onUrlUpdated(mTab);

        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated_whenSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));

        observer.onPageLoadFinished(mTab);

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mTab).isOfflinePage();
        observer.onUrlUpdated(mTab);

        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        disconnect(observer, false);
        showTab(null);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab_whenConnected() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        connect(observer, false);
        showTab(null);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onConnectionTypeChanged() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        assertTrue(observer.isObservingNetworkChanges());

        connect(observer, true);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        assertTrue(observer.isObservingNetworkChanges());

        // Notification comes, but we are still disconnected.
        observer.onConnectionTypeChanged(0);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_tabNotShowing() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        hideTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_pageNotLoaded() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        // That resets the page to not loaded.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_noCurrentTab() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        hideTab(observer);
        // That resets the page to not loaded.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_ignoreEventsAfterShownOnce() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event ignored, snackbar not shown again.
        observer.onPageLoadFinished(mTab);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event ignored, snackbar not shown again.
        observer.onShown(mTab);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event triggers snackbar again.
        observer.onConnectionTypeChanged(0);
        verify(observer, times(2)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTabRemoved_whenNotShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // Snackbar is not showing over here.
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));

        removeTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        // Cannot verify using observer, because of implementation using nested class.
        verify(mTab, times(1)).removeObserver(any(OfflinePageTabObserver.class));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTabRemoved_whenShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab);

        // Snackbar is showing over here.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));

        removeTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        // Cannot verify using observer, because of implementation using nested class.
        verify(mTab, times(1)).removeObserver(any(OfflinePageTabObserver.class));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }
}
