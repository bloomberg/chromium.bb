// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of
 * shared component objects.
 */
public class TabGroupUiCoordinator implements TabGroupUiMediator.ResetHandler, TabGroupUi,
                                              PauseResumeWithNativeObserver,
                                              TabGroupUiMediator.TabGroupUiController {
    static final String COMPONENT_NAME = "TabStrip";
    private final Context mContext;
    private final PropertyModel mModel;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private ChromeActivity mActivity;

    /**
     * Creates a new {@link TabGroupUiCoordinator}
     */
    public TabGroupUiCoordinator(ViewGroup parentView, ThemeColorProvider themeColorProvider) {
        mContext = parentView.getContext();
        mThemeColorProvider = themeColorProvider;
        mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(mContext).inflate(
                R.layout.bottom_tab_strip_toolbar, parentView, false);
        mTabListContainerView = mToolbarView.getViewContainer();
        parentView.addView(mToolbarView);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    @Override
    public void initializeWithNative(ChromeActivity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController) {
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GROUPS_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        assert activity instanceof ChromeTabbedActivity;
        mActivity = activity;
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TabContentManager tabContentManager = activity.getTabContentManager();

        boolean actionOnAllRelatedTabs = TabUiFeatureUtilities.isConditionalTabStripEnabled();
        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, tabModelSelector, null, null, actionOnAllRelatedTabs, null, null,
                TabProperties.UiType.STRIP, null, mTabListContainerView, true, COMPONENT_NAME);
        mTabStripCoordinator.initWithNative(
                mActivity.getCompositorViewHolder().getDynamicResourceLoader());

        mModelChangeProcessor = PropertyModelChangeProcessor.create(mModel,
                new TabGroupUiViewBinder.ViewHolder(
                        mToolbarView, mTabStripCoordinator.getContainerView()),
                TabGroupUiViewBinder::bind);

        // TODO(crbug.com/972217): find a way to enable interactions between grid tab switcher
        //  and the dialog here.
        TabGridDialogMediator.DialogController dialogController = null;
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(mContext, tabModelSelector,
                    tabContentManager, activity, activity.getCompositorViewHolder(), null, null,
                    null, mActivity.getShareDelegateSupplier());
            mTabGridDialogCoordinator.initWithNative(mContext, tabModelSelector, tabContentManager,
                    mTabStripCoordinator.getTabGroupTitleEditor());
            dialogController = mTabGridDialogCoordinator.getDialogController();
        }

        mMediator = new TabGroupUiMediator(activity, visibilityController, this, mModel,
                tabModelSelector, activity,
                ((ChromeTabbedActivity) activity).getOverviewModeBehavior(), mThemeColorProvider,
                dialogController, activity.getLifecycleDispatcher(), activity);

        TabGroupUtils.startObservingForCreationIPH();

        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) return;

        mActivityLifecycleDispatcher = activity.getLifecycleDispatcher();
        mActivityLifecycleDispatcher.register(this);


        // Record the group count after all tabs are being restored. This only happen once per life
        // cycle, therefore remove the observer after recording. We only focus on normal tab model
        // because we don't restore tabs in incognito tab model.
        tabModelSelector.getModel(false).addObserver(new TabModelObserver() {
            @Override
            public void restoreCompleted() {
                recordTabGroupCount();
                recordSessionCount();
                tabModelSelector.getModel(false).removeObserver(this);
            }
        });
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is collapsed or the dialog is hidden.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetGridWithListOfTabs(List<Tab> tabs) {
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.resetWithListOfTabs(tabs);
        }
    }

    /**
     * TabGroupUi implementation.
     */
    @Override
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }
    }

    // PauseResumeWithNativeObserver implementation.
    @Override
    public void onResumeWithNative() {
        // Since we use AsyncTask for restoring tabs, this method can be called before or after
        // restoring all tabs. Therefore, we skip recording the count here during cold start and
        // record that elsewhere when TabModel emits the restoreCompleted signal.
        if (!mActivity.isWarmOnResume()) return;

        recordTabGroupCount();
        recordSessionCount();
    }

    private void recordTabGroupCount() {
        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();
        TabGroupModelFilter normalFilter = (TabGroupModelFilter) provider.getTabModelFilter(false);
        TabGroupModelFilter incognitoFilter =
                (TabGroupModelFilter) provider.getTabModelFilter(true);
        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        RecordHistogram.recordCountHistogram("TabGroups.UserGroupCount", groupCount);
        if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            int namedGroupCount = 0;
            for (int i = 0; i < normalFilter.getTabGroupCount(); i++) {
                int rootId = ((TabImpl) normalFilter.getTabAt(i)).getRootId();
                if (TabGroupUtils.getTabGroupTitle(rootId) != null) {
                    namedGroupCount += 1;
                }
            }
            for (int i = 0; i < incognitoFilter.getTabGroupCount(); i++) {
                int rootId = ((TabImpl) incognitoFilter.getTabAt(i)).getRootId();
                if (TabGroupUtils.getTabGroupTitle(rootId) != null) {
                    namedGroupCount += 1;
                }
            }
            RecordHistogram.recordCountHistogram("TabGroups.UserNamedGroupCount", namedGroupCount);
        }
    }

    private void recordSessionCount() {
        if (mActivity.getOverviewModeBehavior() != null
                && mActivity.getOverviewModeBehavior().overviewVisible()) {
            return;
        }

        Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
        if (currentTab == null) return;
        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();
        ((TabGroupModelFilter) provider.getCurrentTabModelFilter()).recordSessionsCount(currentTab);
    }

    @Override
    public void onPauseWithNative() {}

    // TabGroupUiController implementation.
    @Override
    public void setupLeftButtonDrawable(int drawableId) {
        mMediator.setupLeftButtonDrawable(drawableId);
    }

    @Override
    public void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mMediator.setupLeftButtonOnClickListener(listener);
    }
}
