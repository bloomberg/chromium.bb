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
    private static final int TAB_ID = 77;

    @Mock private Context mContext;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SnackbarController mSnackbarController;
    @Mock private Tab mTab;

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

        // Setting up a mock tab. These are the values common to most tests, but individual
        // tests might easily overwrite them.
        doReturn(TAB_ID).when(mTab).getId();
        doReturn(false).when(mTab).isFrozen();
        doReturn(false).when(mTab).isHidden();
        doReturn(true).when(mTab).isOfflinePage();

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

        doReturn(false).when(mTab).isOfflinePage();
        observer.startObservingTab(mTab);

        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.wasSnackbarShown());

        doReturn(true).when(mTab).isOfflinePage();
        observer.startObservingTab(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStopObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(true).when(mTab).isOfflinePage();
        observer.startObservingTab(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.wasSnackbarShown());

        // Try to stop observing a tab that is not observed.
        doReturn(42).when(mTab).getId();
        observer.stopObservingTab(mTab);

        assertFalse(observer.isObservingTab(mTab));
        doReturn(TAB_ID).when(mTab).getId();
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isObservingNetworkChanges());
        assertFalse(observer.wasSnackbarShown());

        observer.stopObservingTab(mTab);
        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        observer.onShown(mTab);

        verify(observer, times(0)).showReloadSnackbar();
        assertFalse(observer.wasSnackbarShown());

        // Make sure the tab is hidden.
        doReturn(true).when(mTab).isHidden();
        observer.onHidden(mTab);

        // If connected when showing the tab, reload snackbar should be shown as well.
        doReturn(true).when(observer).isConnected();
        observer.onShown(mTab);

        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        doReturn(true).when(mTab).isHidden();
        // Hide the tab.
        observer.onHidden(mTab);

        assertFalse(observer.wasSnackbarShown());
        assertTrue(observer.isObservingTab(mTab));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_afterSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        // Ensure the snackbar is shown first.
        doReturn(true).when(observer).isConnected();
        observer.onShown(mTab);

        doReturn(true).when(mTab).isHidden();
        observer.onHidden(mTab);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnDestroyed() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        doReturn(true).when(observer).isConnected();
        observer.onShown(mTab);

        observer.onDestroyed(mTab);

        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mTab, times(1)).removeObserver(eq(observer));
        assertFalse(observer.wasSnackbarShown());
        assertFalse(observer.isObservingTab(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingTab(mTab));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mTab).isOfflinePage();
        observer.onUrlUpdated(mTab);

        assertFalse(observer.isObservingTab(mTab));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab_whenConnected() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setInstanceForTesting(observer);

        doReturn(true).when(observer).isConnected();

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.wasSnackbarShown());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        // Ensures that observer actually listens for network notifications and tab is shown.
        observer.startObservingTab(mTab);
        observer.onShown(mTab);
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
        observer.startObservingTab(mTab);
        doReturn(true).when(mTab).isHidden();
        observer.onHidden(mTab);

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
        observer.startObservingTab(mTab);
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
        observer.startObservingTab(mTab);
        observer.onShown(mTab);

        doReturn(true).when(observer).isConnected();
        observer.onConnectionTypeChanged(0);

        verify(observer, times(1)).showReloadSnackbar();
        assertTrue(observer.wasSnackbarShown());
        assertTrue(observer.isObservingNetworkChanges());
    }
}
