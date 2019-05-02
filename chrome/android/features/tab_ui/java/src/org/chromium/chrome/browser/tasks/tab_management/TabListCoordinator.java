// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Configuration;
import android.support.annotation.IntDef;
import android.support.annotation.LayoutRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Coordinator for showing UI for a list of tabs. Can be used in GRID or STRIP modes.
 */
public class TabListCoordinator implements Destroyable {
    /** Modes of showing the list of tabs */
    @IntDef({TabListMode.GRID, TabListMode.STRIP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMode {
        int GRID = 0;
        int STRIP = 1;
        int NUM_ENTRIES = 2;
    }

    static final int GRID_LAYOUT_SPAN_COUNT_PORTRAIT = 2;
    static final int GRID_LAYOUT_SPAN_COUNT_LANDSCAPE = 3;
    private final SimpleRecyclerViewMcpBase mModelChangeProcessor;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final @TabListMode int mMode;

    /**
     * Construct a coordinator for UI that shows a list of tabs.
     * @param mode Modes of showing the list of tabs. Can be used in GRID or STRIP.
     * @param context The context to use for accessing {@link android.content.res.Resources}.
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                              the tabs concerned.
     * @param thumbnailProvider Provider to provide screenshot related details.
     * @param titleProvider Provider for a given tab's title.
     * @param createGroupButtonProvider {@link TabListMediator.CreateGroupButtonProvider}
     *         to provide "Create group" button.
     * @param parentView {@link ViewGroup} The root view of the UI.
     * @param attachToParent Whether the UI should attach to root view.
     * @param layoutId ID of the layout resource.
     * @param componentName A unique string uses to identify different components for UMA recording.
     *                      Recommended to use the class name or make sure the string is unique
     *                      through actions.xml file.
     */
    TabListCoordinator(@TabListMode int mode, Context context, TabModelSelector tabModelSelector,
            TabListMediator.ThumbnailProvider thumbnailProvider,
            TabListMediator.TitleProvider titleProvider, boolean closeRelatedTabs,
            @Nullable TabListMediator.CreateGroupButtonProvider createGroupButtonProvider,
            @Nullable TabListMediator
                    .GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @NonNull ViewGroup parentView, boolean attachToParent, @LayoutRes int layoutId,
            String componentName) {
        TabListModel tabListModel = new TabListModel();
        mMode = mode;

        RecyclerViewAdapter adapter;
        if (mMode == TabListMode.GRID) {
            SimpleRecyclerViewMcpBase<PropertyModel, TabGridViewHolder, PropertyKey> mcp =
                    new SimpleRecyclerViewMcpBase<>(
                            null, TabGridViewBinder::onBindViewHolder, tabListModel);
            adapter = new RecyclerViewAdapter<>(mcp, TabGridViewHolder::create);
            mModelChangeProcessor = mcp;
        } else if (mMode == TabListMode.STRIP) {
            SimpleRecyclerViewMcpBase<PropertyModel, TabStripViewHolder, PropertyKey> mcp =
                    new SimpleRecyclerViewMcpBase<>(
                            null, TabStripViewBinder::onBindViewHolder, tabListModel);
            adapter = new RecyclerViewAdapter<>(mcp, TabStripViewHolder::create);
            mModelChangeProcessor = mcp;
        } else {
            throw new IllegalArgumentException(
                    "Attempting to create a tab list UI with invalid mode");
        }
        if (!attachToParent) {
            mRecyclerView = (TabListRecyclerView) LayoutInflater.from(context).inflate(
                    layoutId, parentView, false);
        } else {
            LayoutInflater.from(context).inflate(layoutId, parentView, true);
            mRecyclerView = parentView.findViewById(R.id.tab_list_view);
        }

        mRecyclerView.setAdapter(adapter);

        if (mMode == TabListMode.GRID) {
            mRecyclerView.setLayoutManager(new GridLayoutManager(context,
                    context.getResources().getConfiguration().orientation
                                    == Configuration.ORIENTATION_PORTRAIT
                            ? GRID_LAYOUT_SPAN_COUNT_PORTRAIT
                            : GRID_LAYOUT_SPAN_COUNT_LANDSCAPE));
        } else if (mMode == TabListMode.STRIP) {
            mRecyclerView.setLayoutManager(
                    new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        }

        mRecyclerView.setHasFixedSize(true);

        TabListFaviconProvider tabListFaviconProvider =
                new TabListFaviconProvider(context, Profile.getLastUsedProfile());

        mMediator = new TabListMediator(tabListModel, tabModelSelector, thumbnailProvider,
                titleProvider, tabListFaviconProvider, closeRelatedTabs, createGroupButtonProvider,
                gridCardOnClickListenerProvider, componentName);

        if (mMode == TabListMode.GRID) {
            ItemTouchHelper touchHelper = new ItemTouchHelper(mMediator.getItemTouchHelperCallback(
                    context.getResources().getDimension(R.dimen.swipe_to_dismiss_threshold)));
            touchHelper.attachToRecyclerView(mRecyclerView);
            mMediator.registerOrientationListener(
                    (GridLayoutManager) mRecyclerView.getLayoutManager());
        }
    }

    /**
     * @return The container {@link android.support.v7.widget.RecyclerView} that is showing the
     *         tab list UI.
     */
    public TabListRecyclerView getContainerView() {
        return mRecyclerView;
    }

    /**
     * Reset the tab grid with the given List of Tabs. Can be null.
     * @param tabs List of Tabs to show for in the UI.
     */
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mMediator.resetWithListOfTabs(tabs);

        if (mMode == TabListMode.STRIP && tabs != null && tabs.size() > 1) {
            TabGroupUtils.maybeShowIPH(
                    FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE, mRecyclerView);
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mMediator.destroy();
    }
}
