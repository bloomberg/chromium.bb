// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;

/**
 * This class is a coordinator for TabSelectionEditor component. It manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component.
 */
class TabSelectionEditorCoordinator {
    static final String COMPONENT_NAME = "TabSelectionEditor";

    /**
     * An interface to control the TabSelectionEditor.
     */
    interface TabSelectionEditorController {
        /**
         * Shows the TabSelectionEditor with the given {@link Tab}s.
         * @param tabs List of {@link Tab}s to show.
         */
        void show(List<Tab> tabs);

        /**
         * Shows the TabSelectionEditor with the given {@Link Tab}s, and the first
         * {@code preSelectedTabCount} tabs being selected.
         * @param tabs List of {@link Tab}s to show.
         * @param preSelectedTabCount Number of selected {@link Tab}s.
         */
        void show(List<Tab> tabs, int preSelectedTabCount);

        /**
         * Hides the TabSelectionEditor.
         */
        void hide();

        /**
         * @return Whether or not the TabSelectionEditor consumed the event.
         */
        boolean handleBackPressed();

        /**
         * Configure the Toolbar for TabSelectionEditor. The default button text is "Group".
         * @param actionButtonText Button text for the action button.
         * @param actionProvider The {@link TabSelectionEditorActionProvider} that specifies the
         *         action when action button gets clicked.
         * @param actionButtonEnablingThreshold The minimum threshold to enable the action button.
         *         If it's -1 use the default value.
         * @param navigationButtonOnClickListener Click listener for the navigation button.
         */
        void configureToolbar(@Nullable String actionButtonText,
                @Nullable TabSelectionEditorActionProvider actionProvider,
                int actionButtonEnablingThreshold,
                @Nullable View.OnClickListener navigationButtonOnClickListener);
    }

    private final Context mContext;
    private final View mParentView;
    private final TabModelSelector mTabModelSelector;
    private final TabSelectionEditorLayout mTabSelectionEditorLayout;
    private final TabListCoordinator mTabListCoordinator;
    private final SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();
    private final PropertyModel mModel = new PropertyModel(TabSelectionEditorProperties.ALL_KEYS);
    private final PropertyModelChangeProcessor mTabSelectionEditorLayoutChangeProcessor;
    private final TabSelectionEditorMediator mTabSelectionEditorMediator;

    public TabSelectionEditorCoordinator(Context context, View parentView,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            @Nullable TabSelectionEditorMediator
                    .TabSelectionEditorPositionProvider positionProvider) {
        mContext = context;
        mParentView = parentView;
        mTabModelSelector = tabModelSelector;

        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                mTabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false,
                null, null, null, TabProperties.UiType.SELECTABLE, this::getSelectionDelegate, null,
                null, false, COMPONENT_NAME);
        mTabListCoordinator.registerItemType(TabProperties.UiType.DIVIDER, () -> {
            return LayoutInflater.from(context).inflate(R.layout.divider_preference, null, false);
        }, (model, view, propertyKey) -> {});
        RecyclerView.LayoutManager layoutManager =
                mTabListCoordinator.getContainerView().getLayoutManager();
        if (layoutManager instanceof GridLayoutManager) {
            ((GridLayoutManager) layoutManager)
                    .setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup() {
                        @Override
                        public int getSpanSize(int i) {
                            int itemType = mTabListCoordinator.getContainerView()
                                                   .getAdapter()
                                                   .getItemViewType(i);

                            if (itemType == TabProperties.UiType.DIVIDER) {
                                return ((GridLayoutManager) layoutManager).getSpanCount();
                            }
                            return 1;
                        }
                    });
        }

        mTabSelectionEditorLayout = LayoutInflater.from(context)
                .inflate(R.layout.tab_selection_editor_layout, null)
                .findViewById(R.id.selectable_list);
        mTabSelectionEditorLayout.initialize(mParentView, mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(), mSelectionDelegate);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        mTabSelectionEditorLayoutChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mTabSelectionEditorLayout, TabSelectionEditorLayoutBinder::bind);

        mTabSelectionEditorMediator = new TabSelectionEditorMediator(mContext, mTabModelSelector,
                this::resetWithListOfTabs, mModel, mSelectionDelegate, positionProvider);
    }

    /**
     * @return The {@link SelectionDelegate} that is used in this component.
     */
    SelectionDelegate<Integer> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    /**
     * Resets {@link TabListCoordinator} with the provided list.
     * @param tabs List of {@link Tab}s to reset.
     * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount) {
        mTabListCoordinator.resetWithListOfTabs(tabs);

        if (tabs != null && preSelectedCount > 0) {
            assert preSelectedCount < tabs.size();
            mTabListCoordinator.addSpecialListItem(preSelectedCount, TabProperties.UiType.DIVIDER,
                    new PropertyModel.Builder(CARD_TYPE).with(CARD_TYPE, OTHERS).build());
        }
    }

    /**
     * @return {@link TabSelectionEditorController} that can control the TabSelectionEditor.
     */
    TabSelectionEditorController getController() {
        return mTabSelectionEditorMediator;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mTabSelectionEditorLayout.destroy();
        mTabSelectionEditorMediator.destroy();
        mTabSelectionEditorLayoutChangeProcessor.destroy();
    }
}
