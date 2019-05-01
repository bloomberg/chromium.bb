// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabListMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListMediatorUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String NEW_TITLE = "New title";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;

    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabListFaviconProvider mTabListFaviconProvider;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconCallbackCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private TabListMediator mMediator;
    private TabListModel mModel;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(any(), any(), anyBoolean());
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForUrlAsync(anyString(), anyBoolean(), mCallbackCaptor.capture());

        mModel = new TabListModel();
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                false, null, null, getClass().getSimpleName());
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void updatesTitle() {
        initAndAssertAllProperties();

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        doReturn(NEW_TITLE).when(mTab1).getTitle();
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    public void updatesFavicon() {
        initAndAssertAllProperties();

        assertNotNull(mModel.get(0).get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, null);

        assertNull(mModel.get(0).get(TabProperties.FAVICON));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .get(TabProperties.TAB_SELECTED_LISTENER)
                .run(mModel.get(1).get(TabProperties.TAB_ID));

        verify(mTabModel).setIndex(eq(1), anyInt());
    }

    @Test
    public void sendsCloseSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .get(TabProperties.TAB_CLOSED_LISTENER)
                .run(mModel.get(1).get(TabProperties.TAB_ID));

        verify(mTabModel).closeTab(eq(mTab2), eq(null), eq(false), eq(false), eq(true));
    }

    @Test
    public void tabClosure() {
        initAndAssertAllProperties();

        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().willCloseTab(prepareTab(TAB3_ID, TAB3_TITLE), false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS() {
        initAndAssertAllProperties();
        mMediator.setCloseAllRelatedTabsForTest(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_GTS_Skip() {
        initAndAssertAllProperties();
        mMediator.setCloseAllRelatedTabsForTest(true);

        // Add a new tab to the group with mTab2.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS_Middle() {
        initAndAssertAllProperties();
        mMediator.setCloseAllRelatedTabsForTest(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(newTab).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAdditionEnd() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAdditionMiddle() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabSelection() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabClosureUndone() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(newTab);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    private void initAndAssertAllProperties() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        mMediator.resetWithListOfTabs(tabs);
        for (Callback<Drawable> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(new ColorDrawable(Color.RED));
        }

        assertThat(mModel.size(), equalTo(2));

        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        assertThat(mModel.get(0).get(TabProperties.FAVICON), instanceOf(Drawable.class));
        assertThat(mModel.get(1).get(TabProperties.FAVICON), instanceOf(Drawable.class));

        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(false));

        assertThat(mModel.get(0).get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));
        assertThat(mModel.get(1).get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));

        assertThat(mModel.get(0).get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));

        assertThat(mModel.get(0).get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        return tab;
    }
}
