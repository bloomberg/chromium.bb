// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Bundle;
import android.support.customtabs.CustomTabsSessionToken;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.TabObserverRegistrar;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link CustomTabActivityTabController}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
public class CustomTabActivityTabControllerTest {
    private static final String INITIAL_URL = "https://initial.com";
    private static final String SPECULATED_URL = "https://speculated.com";
    private static final String OTHER_URL = "https://other.com";

    private final Intent mIntent = new Intent();

    @Mock CustomTabDelegateFactory mCustomTabDelegateFactory;
    @Mock ChromeActivity mActivity;
    @Mock CustomTabsConnection mConnection;
    @Mock CustomTabIntentDataProvider mIntentDataProvider;
    @Mock TabContentManager mTabContentManager;
    @Mock TabObserverRegistrar mTabObserverRegistrar;
    @Mock CompositorViewHolder mCompositorViewHolder;
    @Mock WarmupManager mWarmupManager;
    @Mock CustomTabTabPersistencePolicy mTabPersistencePolicy;
    @Mock CustomTabActivityTabFactory mTabFactory;
    @Mock CustomTabObserver mCustomTabObserver;
    @Mock WebContentsFactory mWebContentsFactory;
    @Mock ActivityTabProvider mActivityTabProvider;
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CustomTabsSessionToken mSession;
    @Mock TabModelSelectorImpl mTabModelSelector;
    @Mock TabModel mTabModel;
    @Captor ArgumentCaptor<ActivityTabObserver> mTabObserverCaptor;
    @Captor ArgumentCaptor<WebContents> mWebContentsCaptor;

    private Tab mTabFromFactory;
    private CustomTabActivityTabController mTabController;

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTabFromFactory = prepareTab();

        when(mIntentDataProvider.getIntent()).thenReturn(mIntent);
        when(mIntentDataProvider.getSession()).thenReturn(mSession);
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(INITIAL_URL);
        when(mTabFactory.createTab()).thenReturn(mTabFromFactory);
        when(mTabFactory.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mConnection.getSpeculatedUrl(any())).thenReturn(SPECULATED_URL);

        doNothing().when(mActivityTabProvider).addObserverAndTrigger(mTabObserverCaptor.capture());
        doNothing().when(mTabFromFactory).initialize(mWebContentsCaptor.capture(), any(), any(),
                anyBoolean(), anyBoolean());

        mTabController = new CustomTabActivityTabController(mActivity,
                () -> mCustomTabDelegateFactory, mConnection, mIntentDataProvider,
                () -> mTabContentManager, mActivityTabProvider,
                mTabObserverRegistrar, () -> mCompositorViewHolder, mLifecycleDispatcher,
                mWarmupManager, mTabPersistencePolicy, mTabFactory, () -> mCustomTabObserver,
                mWebContentsFactory);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
        AsyncTabParamsManager.getAsyncTabParams().clear();
    }

    @Test
    public void createsTabEarly_IfWarmUpIsFinished() {
        warmUp();
        mTabController.onPreInflationStartup();
        assertNotNull(mTabController.getTab());
    }

    @Test
    public void startsLoadingPage_InEarlyCreatedTab() {
        warmUp();
        mTabController.onPreInflationStartup();
        verify(mTabFromFactory).loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));
    }

    @Test
    public void requestsWindowFeature_BeforeAddingContent() {
        warmUp();
        mTabController.onPreInflationStartup();
        InOrder inOrder = inOrder(mActivity, mTabFromFactory);
        inOrder.verify(mActivity).supportRequestWindowFeature(anyInt());
        inOrder.verify(mTabFromFactory).loadUrl(any());
    }

    // Some websites replace the tab with a new one.
    @Test
    public void returnsNewTab_IfTabChanges() {
        mTabController.onPreInflationStartup();
        mTabController.onFinishNativeInitialization();
        Tab newTab = prepareTab();
        changeTab(newTab);
        assertEquals(newTab, mTabController.getTab());
    }

    @Test
    public void loadsUrlInNewTab_IfTabChanges() {
        reachNativeInit();
        Tab newTab = mock(Tab.class);
        changeTab(newTab);

        clearInvocations(mTabFromFactory);
        mTabController.loadUrlInTab(mock(LoadUrlParams.class), 0);
        verify(newTab).loadUrl(any());
        verify(mTabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void doesntCreateNewTab_IfRestored() {
        Tab savedTab = prepareTab();
        saveTab(savedTab);
        reachNativeInit();
        verify(mTabFactory, never()).createTab();
    }

    @Test
    public void usesRestoredTab_IfAvailable() {
        Tab savedTab = prepareTab();
        saveTab(savedTab);
        reachNativeInit();
        assertEquals(savedTab, mTabController.getTab());
    }

    @Test
    public void doesntLoadInitialUrl_InRestoredTab() {
        Tab savedTab = prepareTab();
        saveTab(savedTab);
        reachNativeInit();
        verify(savedTab, never()).loadUrl(any());
    }

    @Test
    public void createsANewTabOnNativeInit_IfNoTabExists() {
        reachNativeInit();
        assertEquals(mTabFromFactory, mTabController.getTab());
    }

    @Test
    public void doesntCreateNewTabOnNativeInit_IfCreatedTabEarly() {
        warmUp();
        mTabController.onPreInflationStartup();

        clearInvocations(mTabFactory);
        mTabController.onFinishNativeInitialization();
        verify(mTabFactory, never()).createTab();
    }

    @Test
    public void attachesTabContentManager_IfCreatedTabEarly() {
        warmUp();
        reachNativeInit();
        verify(mTabFromFactory).attachTabContentManager(mTabContentManager);
    }

    @Test
    public void addsEarlyCreatedTab_ToTabModel() {
        warmUp();
        reachNativeInit();
        verify(mTabModel).addTab(eq(mTabFromFactory), anyInt(), anyInt());
    }

    @Test
    public void addsTabCreatedOnNativeInit_ToTabModel() {
        reachNativeInit();
        verify(mTabModel).addTab(eq(mTabFromFactory), anyInt(), anyInt());
    }

    @Test
    public void finishesReparentingHiddenTab_IfExists() {
        Tab hiddenTab = prepareHiddenTab();
        reachNativeInit();
        verify(hiddenTab).attachAndFinishReparenting(any(), any(), any());
    }

    @Test
    public void loadsUrlInHiddenTab_IfExists() {
        Tab hiddenTab = prepareHiddenTab();
        reachNativeInit();
        verify(hiddenTab).loadUrl(any());
    }

    @Test
    public void usesWebContentsCreatedWithWarmRenderer_ByDefault() {
        WebContents webContents = mock(WebContents.class);
        when(mWebContentsFactory.createWebContentsWithWarmRenderer(anyBoolean(), anyBoolean()))
                .thenReturn(webContents);
        reachNativeInit();
        assertEquals(webContents, mWebContentsCaptor.getValue());
    }

    @Test
    public void usesTransferredWebContents_IfAvailable() {
        WebContents transferredWebcontents = prepareTransferredWebcontents();
        reachNativeInit();
        assertEquals(transferredWebcontents, mWebContentsCaptor.getValue());
    }

    @Test
    public void usesSpareWebContents_IfAvailable() {
        WebContents spareWebcontents = prepareSpareWebcontents();
        reachNativeInit();
        assertEquals(spareWebcontents, mWebContentsCaptor.getValue());
    }

    @Test
    public void prefersTransferredWebContents_ToSpareWebContents() {
        WebContents transferredWebcontents = prepareTransferredWebcontents();
        WebContents spareWebcontents = prepareSpareWebcontents();
        reachNativeInit();
        assertEquals(transferredWebcontents, mWebContentsCaptor.getValue());
    }

    @Test
    public void doesntLoadUrl_IfEqualsSpeculatedUrl_AndIsFirstLoad() {
        Tab hiddenTab = prepareHiddenTab();
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(SPECULATED_URL);
        reachNativeInit();
        verify(hiddenTab, never()).loadUrl(any());
    }

    @Test
    public void loadUrl_IfEqualsSpeculatedUrl_ButIsntFirstLoad() {
        Tab hiddenTab = prepareHiddenTab();
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        reachNativeInit();

        clearInvocations(mTabFromFactory);
        LoadUrlParams params = new LoadUrlParams(SPECULATED_URL);
        mTabController.loadUrlInTab(params, 0);
        verify(hiddenTab).loadUrl(params);
    }

    // This is important so that the tab doesn't get hidden, see ChromeActivity#onStopWithNative
    @Test
    public void clearsActiveTab_WhenStartsReparenting() {
        reachNativeInit();
        mTabController.detachAndStartReparenting(new Intent(), new Bundle(), mock(Runnable.class));
        assertNull(mTabController.getTab());
    }

    private void warmUp() {
        when(mConnection.hasWarmUpBeenFinished()).thenReturn(true);
    }

    private void changeTab(Tab newTab) {
        when(mActivityTabProvider.getActivityTab()).thenReturn(newTab);
        for (ActivityTabObserver observer : mTabObserverCaptor.getAllValues()) {
            observer.onActivityTabChanged(newTab, false);
        }
    }

    private void saveTab(Tab tab) {
        when(mActivity.getSavedInstanceState()).thenReturn(new Bundle());
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
    }

    // Dispatches lifecycle events up to native init.
    private void reachNativeInit() {
        mTabController.onPreInflationStartup();
        mTabController.onPostInflationStartup();
        mTabController.onFinishNativeInitialization();
    }

    private WebContents prepareTransferredWebcontents() {
        int tabId = 1;
        WebContents webContents = mock(WebContents.class);
        AsyncTabParamsManager.add(tabId, new AsyncTabCreationParams(mock(LoadUrlParams.class),
                webContents));
        mIntent.putExtra(IntentHandler.EXTRA_TAB_ID, tabId);
        return webContents;
    }

    private WebContents prepareSpareWebcontents() {
        WebContents webContents = mock(WebContents.class);
        when(mWarmupManager.takeSpareWebContents(
                     anyBoolean(), anyBoolean(), eq(WarmupManager.FOR_CCT)))
                .thenReturn(webContents);
        return webContents;
    }

    private Tab prepareHiddenTab() {
        warmUp();
        Tab hiddenTab = prepareTab();
        when(mConnection.takeHiddenTab(any(), any(), any())).thenReturn(hiddenTab);
        return hiddenTab;
    }

    private Tab prepareTab() {
        Tab tab = mock(Tab.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        return tab;
    }
}
