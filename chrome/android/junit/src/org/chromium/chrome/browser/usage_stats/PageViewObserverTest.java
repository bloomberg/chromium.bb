// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;

import java.util.Arrays;

/** Unit tests for PageViewObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class PageViewObserverTest {
    private static final String STARTING_URL = "http://starting.url";
    private static final String DIFFERENT_URL = "http://different.url";
    private static final String STARTING_FQDN = "starting.url";
    private static final String DIFFERENT_FQDN = "different.url";

    @Mock
    private Activity mActivity;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mTabModel;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;
    @Mock
    private EventTracker mEventTracker;
    @Mock
    private TokenTracker mTokenTracker;
    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(false).when(mTab).isIncognito();
        doReturn(null).when(mTab).getUrl();
        doReturn(Arrays.asList(mTabModel)).when(mTabModelSelector).getModels();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
    }

    @Test
    public void createPageViewTimer_withNoTab_noCrash() {
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        PageViewObserver observer = createPageViewObserver();
    }

    @Test
    public void onUpdateUrl_currentlyNull_startReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void onUpdateUrl_nullUrl() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, null);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        verify(mEventTracker, times(0)).addWebsiteEvent(any());
    }

    @Test
    public void onUpdateUrl_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        reset(mEventTracker);
        onUpdateUrl(mTab, DIFFERENT_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void onUpdateUrl_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        onUpdateUrl(mTab, STARTING_URL + "/some_other_page.html");
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        reset(mEventTracker);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));

        doReturn(STARTING_URL).when(mTab2).getUrl();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabHidden_stopReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void tabShown_startReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab).getUrl();
        onShown(mTab, TabSelectionType.FROM_USER);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabClosed_switchToNew_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        onShown(mTab2, TabSelectionType.FROM_CLOSE);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void tabClosed_inBackground_stopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);
        getTabModelObserver().willCloseTab(mTab2, true);
        getTabModelObserver().tabRemoved(mTab2);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStopEvent(DIFFERENT_FQDN)));
    }

    // TODO(pnoland): add test for platform reporting once the System API is available in Q.

    @Test
    public void tabIncognito_eventsNotReported() {
        PageViewObserver observer = createPageViewObserver();
        onUpdateUrl(mTab, STARTING_URL);

        doReturn(true).when(mTab2).isIncognito();
        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStopEvent(DIFFERENT_FQDN)));
    }

    private PageViewObserver createPageViewObserver() {
        PageViewObserver observer =
                new PageViewObserver(mActivity, mTabModelSelector, mEventTracker, mTokenTracker);
        verify(mTabModel, times(1)).addObserver(mTabModelObserverCaptor.capture());
        if (mTabModelSelector.getCurrentTab() != null) {
            verify(mTabModelSelector.getCurrentTab(), times(1))
                    .addObserver(mTabObserverCaptor.capture());
        }
        return observer;
    }

    private void onUpdateUrl(Tab tab, String url) {
        getTabObserver().onUpdateUrl(tab, url);
    }

    private void onHidden(Tab tab, @TabHidingType int hidingType) {
        getTabObserver().onHidden(tab, hidingType);
    }

    private void onShown(Tab tab, @TabSelectionType int selectionType) {
        getTabObserver().onShown(tab, selectionType);
    }

    private void didSelectTab(Tab tab, @TabSelectionType int selectionType) {
        getTabModelObserver().didSelectTab(tab, selectionType, 0);
    }

    private TabObserver getTabObserver() {
        return mTabObserverCaptor.getValue();
    }

    private TabModelObserver getTabModelObserver() {
        return mTabModelObserverCaptor.getValue();
    }

    private ArgumentMatcher<WebsiteEvent> isStartEvent(String fqdn) {
        return new ArgumentMatcher<WebsiteEvent>() {
            @Override
            public boolean matches(WebsiteEvent event) {
                return event.getType() == WebsiteEvent.EventType.START
                        && event.getFqdn().equals(fqdn);
            }

            @Override
            public String toString() {
                return "Start event with fqdn: " + fqdn;
            }
        };
    }

    private ArgumentMatcher<WebsiteEvent> isStopEvent(String fqdn) {
        return new ArgumentMatcher<WebsiteEvent>() {
            @Override
            public boolean matches(WebsiteEvent event) {
                return event.getType() == WebsiteEvent.EventType.STOP
                        && event.getFqdn().equals(fqdn);
            }

            @Override
            public String toString() {
                return "Stop event with fqdn: " + fqdn;
            }
        };
    }
}
