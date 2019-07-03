// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SHADOW_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.graphics.Bitmap;
import android.os.Handler;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The Mediator that is responsible for resetting the tab grid based on visibility and model
 * changes.
 */
class GridTabSwitcherMediator
        implements GridTabSwitcher.GridController, TabListRecyclerView.VisibilityListener {
    // This should be the same as TabListCoordinator.GRID_LAYOUT_SPAN_COUNT for the selected tab
    // to be on the 2nd row.
    static final int INITIAL_SCROLL_INDEX_OFFSET = 2;

    private static final int DEFAULT_TOP_PADDING = 0;

    /** Field trial parameter for the {@link TabListRecyclerView} cleanup delay. */
    private static final String SOFT_CLEANUP_DELAY_PARAM = "soft-cleanup-delay";
    private static final int DEFAULT_SOFT_CLEANUP_DELAY_MS = 3_000;
    private static final String CLEANUP_DELAY_PARAM = "cleanup-delay";
    private static final int DEFAULT_CLEANUP_DELAY_MS = 30_000;
    private Integer mSoftCleanupDelayMsForTesting;
    private Integer mCleanupDelayMsForTesting;
    private final Handler mHandler;
    private final Runnable mSoftClearTabListRunnable;
    private final Runnable mClearTabListRunnable;

    private final ResetHandler mResetHandler;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObserverList<OverviewModeObserver> mObservers = new ObserverList<>();
    private final ChromeFullscreenManager mFullscreenManager;
    private final TabGridDialogMediator.ResetHandler mTabGridDialogResetHandler;
    private final ChromeFullscreenManager.FullscreenListener mFullscreenListener =
            new ChromeFullscreenManager.FullscreenListener() {
                @Override
                public void onContentOffsetChanged(int offset) {}

                @Override
                public void onControlsOffsetChanged(
                        int topOffset, int bottomOffset, boolean needsAnimate) {}

                @Override
                public void onToggleOverlayVideoMode(boolean enabled) {}

                @Override
                public void onBottomControlsHeightChanged(int bottomControlsHeight) {
                    mContainerViewModel.set(BOTTOM_CONTROLS_HEIGHT, bottomControlsHeight);
                }
            };

    private final CompositorViewHolder mCompositorViewHolder;
    private final TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;

    /**
     * In cases where a didSelectTab was due to switching models with a toggle,
     * we don't change tab grid visibility.
     */
    private boolean mShouldIgnoreNextSelect;

    private int mModelIndexWhenShown;
    private int mTabIdwhenShown;
    private int mIndexInNewModelWhenSwitched;

    /**
     * Interface to delegate resetting the tab grid.
     */
    interface ResetHandler {
        /**
         * Reset the tab grid with the given {@link TabList}, which can be null.
         * @param tabList The {@link TabList} to show the tabs for in the grid.
         * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode);

        /**
         * Release the thumbnail {@link Bitmap} but keep the {@link TabGridViewHolder}.
         */
        void softCleanup();
    }

    /**
     * Basic constructor for the Mediator.
     * @param resetHandler The {@link ResetHandler} that handles reset for this Mediator.
     * @param containerViewModel The {@link PropertyModel} to keep state on the View containing the
     *         grid.
     * @param tabModelSelector {@link TabModelSelector} to observer for model and selection changes.
     * @param fullscreenManager {@link FullscreenManager} to use.
     * @param compositorViewHolder {@link CompositorViewHolder} to use.
     * @param tabSelectionEditorController The controller that can control the visibility of the
     *                                     TabSelectionEditor.
     */
    GridTabSwitcherMediator(ResetHandler resetHandler, PropertyModel containerViewModel,
            TabModelSelector tabModelSelector, ChromeFullscreenManager fullscreenManager,
            CompositorViewHolder compositorViewHolder,
            TabGridDialogMediator.ResetHandler tabGridDialogResetHandler,
            TabSelectionEditorCoordinator
                    .TabSelectionEditorController tabSelectionEditorController) {
        mResetHandler = resetHandler;
        mContainerViewModel = containerViewModel;
        mTabModelSelector = tabModelSelector;
        mFullscreenManager = fullscreenManager;

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mShouldIgnoreNextSelect = true;
                mIndexInNewModelWhenSwitched = newModel.index();

                TabList currentTabModelFilter =
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
                mResetHandler.resetWithTabList(currentTabModelFilter, false);
                mContainerViewModel.set(IS_INCOGNITO, currentTabModelFilter.isIncognito());
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, int type) {
                mShouldIgnoreNextSelect = false;
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_CLOSE || mShouldIgnoreNextSelect) {
                    mShouldIgnoreNextSelect = false;
                    return;
                }
                if (mContainerViewModel.get(IS_VISIBLE)) {
                    TabModelFilter modelFilter = mTabModelSelector.getTabModelFilterProvider()
                                                         .getCurrentTabModelFilter();
                    if (modelFilter instanceof TabGroupModelFilter) {
                        ((TabGroupModelFilter) modelFilter).recordSessionsCount(tab);
                    }

                    // Use TabSelectionType.From_USER to filter the new tab creation case.
                    if (type == TabSelectionType.FROM_USER) recordUserSwitchedTab(tab, lastId);

                    hideOverview(
                            !ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION));
                }
            }
        };

        mFullscreenManager.addListener(mFullscreenListener);
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mContainerViewModel.set(VISIBILITY_LISTENER, this);
        mContainerViewModel.set(IS_INCOGNITO,
                mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .isIncognito());
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
        mContainerViewModel.set(TOP_CONTROLS_HEIGHT, fullscreenManager.getTopControlsHeight());
        mContainerViewModel.set(
                BOTTOM_CONTROLS_HEIGHT, fullscreenManager.getBottomControlsHeight());

        int toolbarHeight =
                ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                        R.dimen.toolbar_height_no_shadow);
        int topPadding = ReturnToChromeExperimentsUtil.shouldShowOmniboxOnTabSwitcher()
                ? toolbarHeight
                : DEFAULT_TOP_PADDING;
        mContainerViewModel.set(TOP_PADDING, topPadding);
        int shadowTopMargin = ReturnToChromeExperimentsUtil.shouldShowOmniboxOnTabSwitcher()
                ? toolbarHeight * 2
                : toolbarHeight;
        mContainerViewModel.set(SHADOW_TOP_MARGIN, shadowTopMargin);

        mCompositorViewHolder = compositorViewHolder;
        mTabGridDialogResetHandler = tabGridDialogResetHandler;

        mSoftClearTabListRunnable = mResetHandler::softCleanup;
        mClearTabListRunnable = () -> mResetHandler.resetWithTabList(null, false);
        mHandler = new Handler();
        mTabSelectionEditorController = tabSelectionEditorController;
    }

    private int getSoftCleanupDelay() {
        if (mSoftCleanupDelayMsForTesting != null) return mSoftCleanupDelayMsForTesting;

        String delay = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SOFT_CLEANUP_DELAY_PARAM);
        try {
            return Integer.valueOf(delay);
        } catch (NumberFormatException e) {
            return DEFAULT_SOFT_CLEANUP_DELAY_MS;
        }
    }

    private int getCleanupDelay() {
        if (mCleanupDelayMsForTesting != null) return mCleanupDelayMsForTesting;

        String delay = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, CLEANUP_DELAY_PARAM);
        try {
            return Integer.valueOf(delay);
        } catch (NumberFormatException e) {
            return DEFAULT_CLEANUP_DELAY_MS;
        }
    }

    private void setVisibility(boolean isVisible) {
        if (isVisible) {
            RecordUserAction.record("MobileToolbarShowStackView");
        }

        mContainerViewModel.set(IS_VISIBLE, isVisible);
    }

    private void setContentOverlayVisibility(boolean isVisible) {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        mCompositorViewHolder.setContentOverlayVisibility(isVisible, true);
    }

    /**
     * Record tab switch related metric for GTS.
     * @param tab The new selected tab.
     * @param lastId The id of the previous selected tab, and that tab is still a valid tab
     *               in TabModel.
     */
    private void recordUserSwitchedTab(Tab tab, int lastId) {
        Tab fromTab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), lastId);
        assert fromTab != null;

        if (mModelIndexWhenShown == mTabModelSelector.getCurrentModelIndex()) {
            if (tab.getId() == mTabIdwhenShown) {
                RecordUserAction.record("MobileTabReturnedToCurrentTab");
                RecordHistogram.recordSparseHistogram(
                        "Tabs.TabOffsetOfSwitch." + GridTabSwitcherCoordinator.COMPONENT_NAME, 0);
            } else {
                int fromIndex = mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .indexOf(fromTab);
                int toIndex = mTabModelSelector.getTabModelFilterProvider()
                                      .getCurrentTabModelFilter()
                                      .indexOf(tab);

                if (fromIndex != toIndex || fromTab.getId() == tab.getId()) {
                    RecordUserAction.record(
                            "MobileTabSwitched." + GridTabSwitcherCoordinator.COMPONENT_NAME);
                    RecordHistogram.recordSparseHistogram(
                            "Tabs.TabOffsetOfSwitch." + GridTabSwitcherCoordinator.COMPONENT_NAME,
                            fromIndex - toIndex);
                }
            }
        } else {
            int newSelectedTabIndex =
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tab.getId());
            if (newSelectedTabIndex == mIndexInNewModelWhenSwitched) {
                // TabModelImpl logs this action only when a different index is set within a
                // TabModelImpl. If we switch between normal tab model and incognito tab model and
                // leave the index the same (i.e. after switched tab model and select the
                // highlighted tab), TabModelImpl doesn't catch this case. Therefore, we record it
                // here.
                RecordUserAction.record("MobileTabSwitched");
            }
            RecordUserAction.record(
                    "MobileTabSwitched." + GridTabSwitcherCoordinator.COMPONENT_NAME);
        }
    }

    @Override
    public boolean overviewVisible() {
        return mContainerViewModel.get(IS_VISIBLE);
    }

    @Override
    public void addOverviewModeObserver(OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(false);
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    boolean prepareOverview() {
        mHandler.removeCallbacks(mSoftClearTabListRunnable);
        mHandler.removeCallbacks(mClearTabListRunnable);
        boolean quick = false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)) {
            quick = mResetHandler.resetWithTabList(
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                    false);
        }
        int initialPosition = Math.max(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter().index()
                        - INITIAL_SCROLL_INDEX_OFFSET,
                0);
        mContainerViewModel.set(INITIAL_SCROLL_INDEX, initialPosition);
        return quick;
    }

    @Override
    public void showOverview(boolean animate) {
        mResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION));
        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(true);
        mModelIndexWhenShown = mTabModelSelector.getCurrentModelIndex();
        mTabIdwhenShown = mTabModelSelector.getCurrentTabId();
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    @Override
    public void startedShowing(boolean isAnimating) {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeStartedShowing(true);
        }
    }

    @Override
    public void finishedShowing() {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeFinishedShowing();
        }
        setContentOverlayVisibility(false);
    }

    @Override
    public void startedHiding(boolean isAnimating) {
        setContentOverlayVisibility(true);
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeStartedHiding(true, false);
        }
    }

    @Override
    public void finishedHiding() {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeFinishedHiding();
        }
    }

    @Override
    public boolean onBackPressed() {
        if (!mContainerViewModel.get(IS_VISIBLE)) return false;
        if (mTabSelectionEditorController.handleBackPressed()) return true;

        recordUserSwitchedTab(
                mTabModelSelector.getCurrentTab(), mTabModelSelector.getCurrentTabId());
        hideOverview(!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION));
        return true;
    }

    /**
     * Do clean-up work after the overview hiding animation is finished.
     * @see GridTabSwitcher#postHiding
     */
    void postHiding() {
        mHandler.postDelayed(mSoftClearTabListRunnable, getSoftCleanupDelay());
        mHandler.postDelayed(mClearTabListRunnable, getCleanupDelay());
    }

    /**
     * Set the delay for soft cleanup.
     */
    void setSoftCleanupDelayForTesting(int timeoutMs) {
        mSoftCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Set the delay for lazy cleanup.
     */
    void setCleanupDelayForTesting(int timeoutMs) {
        mCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mFullscreenManager.removeListener(mFullscreenListener);
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
    }

    @Nullable
    TabListMediator.TabActionListener getGridCardOnClickListener(Tab tab) {
        if (!ableToOpenDialog(tab)) return null;
        return tabId -> {
            mTabGridDialogResetHandler.resetWithListOfTabs(getRelatedTabs(tabId));
        };
    }

    @Nullable
    TabListMediator.TabActionListener getCreateGroupButtonOnClickListener(Tab tab) {
        if (!ableToCreateGroup(tab) || FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled())
            return null;

        return tabId -> {
            Tab parentTab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);
            mTabModelSelector.getCurrentModel().commitAllTabClosures();
            mTabModelSelector.openNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                    TabLaunchType.FROM_CHROME_UI, parentTab,
                    mTabModelSelector.isIncognitoSelected());
            RecordUserAction.record("TabGroup.Created.TabSwitcher");
        };
    }

    private boolean ableToCreateGroup(Tab tab) {
        return FeatureUtilities.isTabGroupsAndroidEnabled()
                && mTabModelSelector.isIncognitoSelected() == tab.isIncognito()
                && getRelatedTabs(tab.getId()).size() == 1;
    }

    private boolean ableToOpenDialog(Tab tab) {
        return FeatureUtilities.isTabGroupsAndroidEnabled()
                && mTabModelSelector.isIncognitoSelected() == tab.isIncognito()
                && getRelatedTabs(tab.getId()).size() != 1;
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }
}
