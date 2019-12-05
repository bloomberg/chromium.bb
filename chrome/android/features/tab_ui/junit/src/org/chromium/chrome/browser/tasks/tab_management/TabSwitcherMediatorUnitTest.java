// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
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
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabSwitcherMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
@DisableFeatures(
        {ChromeFeatureList.TAB_SWITCHER_ON_RETURN, ChromeFeatureList.START_SURFACE_ANDROID})
@EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class TabSwitcherMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int CONTROL_HEIGHT_DEFAULT = 56;
    private static final int CONTROL_HEIGHT_MODIFIED = 0;

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;

    @Mock
    TabSwitcherMediator.ResetHandler mResetHandler;
    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    ChromeFullscreenManager mFullscreenManager;
    @Mock
    PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock
    TabSwitcher.OverviewModeObserver mOverviewModeObserver;
    @Mock
    CompositorViewHolder mCompositorViewHolder;
    @Mock
    Layout mLayout;
    @Mock
    TabGridDialogMediator.DialogController mTabGridDialogController;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    ArgumentCaptor<ChromeFullscreenManager.FullscreenListener> mFullscreenListenerCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private TabSwitcherMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = prepareTab(TAB3_ID, TAB3_TITLE);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(any(), any(), anyBoolean(), anyBoolean());
        doReturn(mResources).when(mContext).getResources();

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab3).when(mTabModelFilter).getTabAt(2);
        doReturn(false).when(mTabModelFilter).isIncognito();
        doReturn(2).when(mTabModelFilter).index();
        doReturn(3).when(mTabModelFilter).getCount();

        doReturn(2).when(mTabModel).index();
        doReturn(3).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab3).when(mTabModel).getTabAt(2);

        doReturn(CONTROL_HEIGHT_DEFAULT).when(mFullscreenManager).getBottomControlsHeight();
        doReturn(CONTROL_HEIGHT_DEFAULT).when(mFullscreenManager).getTopControlsHeight();
        doNothing().when(mFullscreenManager).addListener(mFullscreenListenerCaptor.capture());

        mModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);
        mModel.addObserver(mPropertyObserver);
        mMediator = new TabSwitcherMediator(mResetHandler, mModel, mTabModelSelector,
                mFullscreenManager, mCompositorViewHolder, null, null,
                TabListCoordinator.TabListMode.GRID);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);
        mMediator.setOnTabSelectingListener(mLayout::onTabSelecting);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(false);
    }

    @Test
    public void initializesWithCurrentModelOnCreation() {
        initAndAssertAllProperties();
    }

    @Test
    public void showsWithAnimation() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);

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

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
        assertThat(mMediator.overviewVisible(), equalTo(true));

        assertThat(ShadowRecordHistogram.getHistogramValueCountForTesting(
                           TabSwitcherMediator.TAB_COUNT_HISTOGRAM, 3),
                equalTo(1));
        assertThat(ShadowRecordHistogram.getHistogramValueCountForTesting(
                           TabSwitcherMediator.TAB_ENTRIES_HISTOGRAM, 3),
                equalTo(1));
    }

    @Test
    public void showsWithoutAnimation_withTabGroups() {
        doReturn(2).when(mTabModelFilter).getCount();

        initAndAssertAllProperties();
        mMediator.showOverview(false);

        InOrder inOrder = inOrder(mPropertyObserver, mPropertyObserver);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.IS_VISIBLE);
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(mModel, TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
        assertThat(mMediator.overviewVisible(), equalTo(true));

        assertThat(ShadowRecordHistogram.getHistogramValueCountForTesting(
                           TabSwitcherMediator.TAB_COUNT_HISTOGRAM, 3),
                equalTo(1));
        assertThat(ShadowRecordHistogram.getHistogramValueCountForTesting(
                           TabSwitcherMediator.TAB_ENTRIES_HISTOGRAM, 2),
                equalTo(1));
    }

    @Test
    public void hidesWithAnimation() {
        initAndAssertAllProperties();
        mMediator.setTabGridDialogController(mTabGridDialogController);
        mMediator.showOverview(true);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mMediator.hideOverview(true);

        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
        assertThat(mMediator.overviewVisible(), equalTo(false));
        verify(mTabGridDialogController).hideDialog(eq(false));
    }

    @Test
    public void hidesWithoutAnimation() {
        initAndAssertAllProperties();
        mMediator.setTabGridDialogController(mTabGridDialogController);
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
        verify(mTabGridDialogController).hideDialog(eq(false));
    }

    @Test
    public void startedShowingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).startedShowing(true);
        verify(mOverviewModeObserver).startedShowing();
    }

    @Test
    public void finishedShowingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).finishedShowing();
        verify(mOverviewModeObserver).finishedShowing();
    }

    @Test
    public void startedHidingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).startedHiding(true);
        verify(mOverviewModeObserver).startedHiding();
    }

    @Test
    public void finishedHidingPropagatesToObservers() {
        initAndAssertAllProperties();
        mModel.get(TabListContainerProperties.VISIBILITY_LISTENER).finishedHiding();
        verify(mOverviewModeObserver).finishedHiding();
    }

    @Test
    public void resetsToNullAfterHidingFinishes() {
        initAndAssertAllProperties();
        mMediator.setSoftCleanupDelayForTesting(0);
        mMediator.setCleanupDelayForTesting(0);
        mMediator.postHiding();
        verify(mResetHandler).softCleanup();
        verify(mResetHandler).resetWithTabList(eq(null), eq(false), eq(false));
    }

    @Test
    public void resetsAfterNewTabModelSelected_DialogEnabled() {
        initAndAssertAllProperties();
        mModel.set(TabListContainerProperties.IS_VISIBLE, true);

        // Setup dialog reset handler. Default setup is that dialog handler is null.
        mMediator.setTabGridDialogController(mTabGridDialogController);

        doReturn(true).when(mTabModelFilter).isIncognito();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        verify(mResetHandler).resetWithTabList(eq(mTabModelFilter), eq(false), eq(false));
        verify(mTabGridDialogController).hideDialog(eq(false));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO), equalTo(true));

        // Switching TabModels by itself shouldn't cause visibility changes.
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void resetsAfterNewTabModelSelected_DialogNotEnabled() {
        initAndAssertAllProperties();
        mModel.set(TabListContainerProperties.IS_VISIBLE, true);

        doReturn(true).when(mTabModelFilter).isIncognito();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        verify(mResetHandler).resetWithTabList(eq(mTabModelFilter), eq(false), eq(false));
        verify(mTabGridDialogController, never()).hideDialog(eq(false));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO), equalTo(true));

        // Switching TabModels by itself shouldn't cause visibility changes.
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void noResetsAfterNewTabModelSelected_InvisibleGTS() {
        initAndAssertAllProperties();
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));

        // Setup dialog reset handler. Default setup is that dialog handler is null.
        mMediator.setTabGridDialogController(mTabGridDialogController);

        doReturn(true).when(mTabModelFilter).isIncognito();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        verify(mResetHandler, never()).resetWithTabList(any(), anyBoolean(), anyBoolean());
        verify(mTabGridDialogController).hideDialog(eq(false));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO), equalTo(true));

        // Switching TabModels by itself shouldn't cause visibility changes.
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void updatesMarginWithBottomBarChanges() {
        initAndAssertAllProperties();

        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));

        mFullscreenListenerCaptor.getValue().onBottomControlsHeightChanged(
                CONTROL_HEIGHT_MODIFIED, 0);
        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_MODIFIED));
    }

    @Test
    public void hidesWhenATabIsSelected() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);

        verify(mLayout).onTabSelecting(anyLong(), eq(TAB1_ID));
    }

    @Test
    public void doesNotHideWhenSelectedTabChangedDueToTabClosure() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        doReturn(true).when(mTab3).isClosing();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_CLOSE, TAB3_ID);
        verify(mLayout, never()).onTabSelecting(anyLong(), anyInt());

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        verify(mLayout).onTabSelecting(anyLong(), eq(TAB1_ID));
    }

    @Test
    public void doesNotHideWhenSelectedTabChangedDueToModelChange() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mTabModel, null);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        verify(mLayout, never()).onTabSelecting(anyLong(), anyInt());

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB3_ID);
        verify(mLayout).onTabSelecting(anyLong(), eq(TAB1_ID));
    }

    @Test
    public void updatesResetHandlerOnRestoreCompleted() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().restoreCompleted();

        // MRU will be false unless the start surface is enabled.
        verify(mResetHandler).resetWithTabList(mTabModelFilter, false, false);
    }

    @Test
    public void setInitialScrollIndexOnRestoreCompleted() {
        initAndAssertAllProperties();
        mMediator.showOverview(true);
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(true));

        mModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 1);

        mTabModelObserverCaptor.getValue().restoreCompleted();
        assertThat(mModel.get(TabListContainerProperties.INITIAL_SCROLL_INDEX), equalTo(0));
    }

    @Test
    public void showOverviewDoesNotUpdateResetHandlerBeforeRestoreCompleted() {
        initAndAssertAllProperties();
        doReturn(false).when(mTabModelFilter).isTabModelRestored();
        mMediator.showOverview(true);

        // MRU will be false unless the start surface is enabled.
        verify(mResetHandler, never()).resetWithTabList(mTabModelFilter, true, false);
    }

    @Test
    public void prepareOverviewDoesNotUpdateResetHandlerBeforeRestoreCompleted() {
        initAndAssertAllProperties();
        doReturn(false).when(mTabModelFilter).isTabModelRestored();
        mMediator.prepareOverview();

        // MRU will be false unless the start surface is enabled.
        verify(mResetHandler, never()).resetWithTabList(mTabModelFilter, false, false);
    }

    @Test
    public void showOverviewUpdatesResetHandlerAfterRestoreCompleted() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelFilter).isTabModelRestored();

        mMediator.showOverview(true);

        // MRU will be false unless the start surface is enabled.
        verify(mResetHandler).resetWithTabList(mTabModelFilter, true, false);
    }

    @Test
    public void prepareOverviewUpdatesResetHandlerAfterRestoreCompleted() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelFilter).isTabModelRestored();

        mMediator.prepareOverview();

        // MRU will be false unless the start surface is enabled.
        verify(mResetHandler).resetWithTabList(mTabModelFilter, false, false);
    }

    @Test
    public void prepareOverviewSetsInitialScrollIndexAfterRestoreCompleted() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelFilter).isTabModelRestored();

        mModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 1);

        mMediator.prepareOverview();
        assertThat(mModel.get(TabListContainerProperties.INITIAL_SCROLL_INDEX), equalTo(0));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID)
    public void openDialogButton_FlagDisabled() {
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
        // Set up a tab group.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_TITLE);
        doReturn(new ArrayList<>(Arrays.asList(mTab1, newTab)))
                .when(mTabModelFilter)
                .getRelatedTabList(TAB1_ID);

        assertThat(mMediator.openTabGridDialog(mTab1), equalTo(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID)
    public void openDialogButton_SingleTab() {
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mMediator.setTabGridDialogController(mTabGridDialogController);
        // Mock that tab 1 is a single tab.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabModelFilter)
                .getRelatedTabList(TAB1_ID);
        assertThat(mMediator.openTabGridDialog(mTab1), equalTo(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID)
    public void openDialogButton_TabGroup_NotEmpty() {
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mMediator.setTabGridDialogController(mTabGridDialogController);
        // Set up a tab group.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(TAB1_ID);

        TabListMediator.TabActionListener listener = mMediator.openTabGridDialog(mTab1);
        assertThat(listener, notNullValue());

        listener.run(TAB1_ID);
        verify(mTabGridDialogController).resetWithListOfTabs(eq(tabs));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID)
    public void openDialogButton_TabGroup_Empty() {
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mMediator.setTabGridDialogController(mTabGridDialogController);
        // Assume that due to tab model change, current group becomes empty in current model.
        doReturn(new ArrayList<>()).when(mTabModelFilter).getRelatedTabList(TAB1_ID);

        TabListMediator.TabActionListener listener = mMediator.openTabGridDialog(mTab1);
        assertThat(listener, notNullValue());

        listener.run(TAB1_ID);
        verify(mTabGridDialogController).resetWithListOfTabs(eq(null));
    }

    private void initAndAssertAllProperties() {
        assertThat(mModel.get(TabListContainerProperties.VISIBILITY_LISTENER),
                instanceOf(TabSwitcherMediator.class));
        assertThat(
                mModel.get(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES), equalTo(true));
        assertThat(mModel.get(TabListContainerProperties.IS_INCOGNITO),
                equalTo(mTabModel.isIncognito()));
        assertThat(mModel.get(TabListContainerProperties.IS_VISIBLE), equalTo(false));
        assertThat(mModel.get(TabListContainerProperties.TOP_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));
        assertThat(mModel.get(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT),
                equalTo(CONTROL_HEIGHT_DEFAULT));
    }

    private TabImpl prepareTab(int id, String title) {
        TabImpl tab = mock(TabImpl.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(false).when(tab).isClosing();
        return tab;
    }
}
