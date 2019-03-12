// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcherMediator.INITIAL_SCROLL_INDEX_OFFSET;

import android.content.Context;
import android.content.res.Resources;
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
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link GridTabSwitcherMediatorUnitTest}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GridTabSwitcherMediatorUnitTest {
    private static final int CONTROL_HEIGHT_DEFAULT = 56;
    private static final int CONTROL_HEIGHT_MODIFIED = 0;

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String NEW_TITLE = "New title";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;

    @Mock
    GridTabSwitcherMediator.ResetHandler mResetHandler;
    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModel mTabModel;
    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    ChromeFullscreenManager mFullscreenManager;
    @Mock
    PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock
    OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    ArgumentCaptor<ChromeFullscreenManager.FullscreenListener> mFullscreenListenerCaptor;
    @Captor
    ArgumentCaptor<Tab> mTabCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private GridTabSwitcherMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = prepareTab(TAB3_ID, TAB3_TITLE);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing().when(mTabContentManager).getTabThumbnailWithCallback(any(), any());
        doReturn(mResources).when(mContext).getResources();

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab2).when(mTabModel).getTabAt(2);
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(2).when(mTabModel).index();
        doReturn(3).when(mTabModel).getCount();

        doReturn(CONTROL_HEIGHT_DEFAULT).when(mFullscreenManager).getBottomControlsHeight();
        doReturn(CONTROL_HEIGHT_DEFAULT).when(mFullscreenManager).getTopControlsHeight();
        doNothing().when(mFullscreenManager).addListener(mFullscreenListenerCaptor.capture());

        mModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);
        mModel.addObserver(mPropertyObserver);
        mMediator = new GridTabSwitcherMediator(
                mResetHandler, mModel, mTabModelSelector, mFullscreenManager);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void initializesWithCurrentModelOnCreation() {
        initAndAssertAllProperties();
    }

    @Test
    public void showsWithAnimation() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);

        verify(mResetHandler).resetWithTabModel(mTabModel);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
        assertThat(mMediator.overviewVisible(), equalTo(true));
    }

    @Test
    public void showsWithoutAnimation() {
        initAndAssertAllProperties();
        mMediator.showOverview(false);

        InOrder inOrder = inOrder(mPropertyObserver, mPropertyObserver);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.IS_VISIBLE);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);

        verify(mResetHandler).resetWithTabModel(mTabModel);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
        assertThat(mMediator.overviewVisible(), equalTo(true));
    }

    @Test
    public void hidesWithAnimation() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mMediator.hideOverview(true);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
        assertThat(mMediator.overviewVisible(), equalTo(false));
    }

    @Test
    public void hidesWithoutAnimation() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mMediator.hideOverview(false);

        InOrder inOrder = inOrder(mPropertyObserver, mPropertyObserver);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.IS_VISIBLE);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
        assertThat(mMediator.overviewVisible(), equalTo(false));
    }

    @Test
    public void startedShowingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).startedShowing(true);
        verify(mOverviewModeObserver).onOverviewModeStartedShowing(eq(true));
    }

    @Test
    public void finishedShowingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).finishedShowing();
        verify(mOverviewModeObserver).onOverviewModeFinishedShowing();
    }

    @Test
    public void startedHidingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).startedHiding(true);
        verify(mOverviewModeObserver).onOverviewModeStartedHiding(eq(true), eq(false));
    }

    @Test
    public void finishedHidingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).finishedHiding();
        verify(mOverviewModeObserver).onOverviewModeFinishedHiding();
    }

    @Test
    public void resetsToNullAfterHidingFinishes() {
        initAndAssertAllProperties();
        mMediator.finishedHiding();
        verify(mResetHandler).resetWithTabModel(eq(null));
        assertThat(mModel.get(TabListContainerProperties.INITIAL_SCROLL_INDEX), equalTo(0));
    }

    @Test
    public void resetsAfterNewTabModelSelected() {
        initAndAssertAllProperties();

        doReturn(true).when(mTabModel).isIncognito();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        verify(mResetHandler).resetWithTabModel(eq(mTabModel));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO), equalTo(true));

        // Switching TabModels by itself shouldn't cause visibility changes.
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void updatesMarginWithBottomBarChanges() {
        initAndAssertAllProperties();

        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));

        mFullscreenListenerCaptor.getValue().onBottomControlsHeightChanged(CONTROL_HEIGHT_MODIFIED);
        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_MODIFIED));
    }

    @Test
    public void hidesWhenATabIsSelected() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);

        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void doesNotHideWhenSelectedTabChangedDueToTabClosure() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        doReturn(true).when(mTab3).isClosing();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_CLOSE, TAB3_ID);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void doesNotHideWhenSelectedTabChangedDueToModelChange() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
    }

    private void initAndAssertAllProperties() {
        assertThat(mModel.get(TabListContainerProperties.VISIBILITY_LISTENER),
                instanceOf(GridTabSwitcherMediator.class));
        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO),
                equalTo(mTabModel.isIncognito()));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
        assertThat(mModel.get(TabListContainerProperties.TOP_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));
        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));
        assertThat(mModel.get(TabListContainerProperties.INITIAL_SCROLL_INDEX),
                equalTo(Math.max(mTabModel.index() - INITIAL_SCROLL_INDEX_OFFSET, 0)));
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(false).when(tab).isClosing();
        return tab;
    }
}
