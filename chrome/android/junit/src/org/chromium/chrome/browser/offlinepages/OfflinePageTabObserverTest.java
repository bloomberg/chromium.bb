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

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.shadows.ShadowMultiDex;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

/**
 * Unit tests for OfflinePageUtils.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowMultiDex.class})
public class OfflinePageTabObserverTest {
    // Using a null tab, as it cannot be mocked. TabHelper will help return proper mocked responses.
    private static final Tab TAB = null;
    private static final int TAB_ID = 77;

    @Mock private Context mContext;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SnackbarController mSnackbarController;
    @Mock private OfflinePageTabObserver.TabHelper mTabHelper;

    private OfflinePageTabObserver createObserver() {
        OfflinePageTabObserver observer =
                spy(new OfflinePageTabObserver(mContext, mSnackbarManager, mSnackbarController));
        // Mocking out all of the calls that touch on NetworkChangeNotifier, which we cannot
        // directly mock out.
        doNothing().when(observer).startObservingNetworkChanges();
        doNothing().when(observer).stopObservingNetworkChanges();
        // TODO(fgorski): This call has to be mocked out until we update OfflinePageUtils.
        // It also goes to NetworkChangeNotifier from there.
        doReturn(false).when(observer).isConnected();
        // TODO(fgorski): This call has to be mocked out until we update OfflinePageUtils.
        doNothing().when(observer).showReloadSnackbar();
        return observer;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        // Setting up a mock tab helper. These are the values common to most tests, but individual
        // tests might easily overwrite them.
        doReturn(TAB_ID).when(mTabHelper).getTabId(any(Tab.class));
        doReturn(true).when(mTabHelper).isTabShowing(any(Tab.class));
        doReturn(true).when(mTabHelper).isOfflinePage(any(Tab.class));
        OfflinePageTabObserver.setTabHelperForTesting(mTabHelper);

        // Setting up mock snackbar manager.
        doNothing().when(mSnackbarManager).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testBasicState() {
        OfflinePageTabObserver observer = createObserver();
        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(false).when(mTabHelper).isOfflinePage(any(Tab.class));
        observer.startObservingTab(TAB);

        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(TAB));
        assertFalse(observer.wasSnackbarShown());

        doReturn(true).when(mTabHelper).isOfflinePage(any(Tab.class));
        observer.startObservingTab(TAB);

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(TAB));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStopObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(true).when(mTabHelper).isOfflinePage(any(Tab.class));
        observer.startObservingTab(TAB);

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(TAB));
        assertFalse(observer.wasSnackbarShown());

        // Try to stop observing a tab that is not observed.
        doReturn(42).when(mTabHelper).getTabId(any(Tab.class));
        observer.stopObservingTab(TAB);

        assertFalse(observer.isObservingTab(TAB));
        doReturn(TAB_ID).when(mTabHelper).getTabId(any(Tab.class));
        assertTrue(observer.isObservingTab(TAB));
        assertTrue(observer.isObservingNetworkChanges());
        assertFalse(observer.wasSnackbarShown());

        observer.stopObservingTab(TAB);
        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(TAB));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(TAB);
        observer.onShown(TAB);

        verify(observer, times(0)).showReloadSnackbar();
        assertFalse(observer.wasSnackbarShown());

        // Make sure the tab is hidden.
        doReturn(false).when(mTabHelper).isTabShowing(any(Tab.class));
        observer.onHidden(TAB);

        // If connected when showing the tab, reload snackbar should be shown as well.
        doReturn(true).when(observer).isConnected();
        observer.onShown(TAB);

        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(TAB);
        doReturn(false).when(mTabHelper).isTabShowing(any(Tab.class));
        // Hide the tab.
        observer.onHidden(TAB);

        assertFalse(observer.wasSnackbarShown());
        assertTrue(observer.isObservingTab(TAB));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_afterSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(TAB);
        // Ensure the snackbar is shown first.
        doReturn(true).when(observer).isConnected();
        observer.onShown(TAB);

        doReturn(false).when(mTabHelper).isTabShowing(any(Tab.class));
        observer.onHidden(TAB);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnDestroyed() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(TAB);
        doReturn(true).when(observer).isConnected();
        observer.onShown(TAB);

        observer.onDestroyed(TAB);

        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mTabHelper, times(1)).removeObserver(any(Tab.class), eq(observer));
        assertFalse(observer.wasSnackbarShown());
        assertFalse(observer.isObservingTab(TAB));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(TAB);
        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(TAB);

        assertTrue(observer.isObservingTab(TAB));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mTabHelper).isOfflinePage(any(Tab.class));
        observer.onUrlUpdated(TAB);

        assertFalse(observer.isObservingTab(TAB));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(TAB);

        verify(mTabHelper, times(1)).addObserver(any(Tab.class), eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        assertTrue(observer.isObservingTab(TAB));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab_whenConnected() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        doReturn(true).when(observer).isConnected();

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(TAB);

        verify(mTabHelper, times(1)).addObserver(any(Tab.class), eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        assertTrue(observer.isObservingTab(TAB));
        assertTrue(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        // Ensures that observer actually listens for network notifications and tab is shown.
        observer.startObservingTab(TAB);
        observer.onShown(TAB);
        assertTrue(observer.isObservingNetworkChanges());

        // Notification comes, but we are still disconnected.
        observer.onConnectionTypeChanged(0);

        verify(observer, times(0)).showReloadSnackbar();
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_tabNotShowing() {
        OfflinePageTabObserver observer = createObserver();

        // Ensures that observer actually listens for network notifications and tab is hidden.
        observer.startObservingTab(TAB);
        doReturn(false).when(mTabHelper).isTabShowing(any(Tab.class));
        observer.onHidden(TAB);

        doReturn(true).when(observer).isConnected();
        assertTrue(observer.isObservingNetworkChanges());
        observer.onConnectionTypeChanged(0);

        verify(observer, times(0)).showReloadSnackbar();
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_wasSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(true).when(observer).isConnected();

        // Ensures that observer actually listens for network notifications and tab is shown when
        // connected, which means snackbar is shown as well.
        observer.startObservingTab(TAB);
        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());

        assertTrue(observer.isObservingNetworkChanges());
        observer.onConnectionTypeChanged(0);
        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged() {
        OfflinePageTabObserver observer = createObserver();

        // Ensures that observer actually listens for network notifications and tab is shown.
        observer.startObservingTab(TAB);
        observer.onShown(TAB);

        doReturn(true).when(observer).isConnected();
        observer.onConnectionTypeChanged(0);

        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());
        assertTrue(observer.isObservingNetworkChanges());
    }
}
