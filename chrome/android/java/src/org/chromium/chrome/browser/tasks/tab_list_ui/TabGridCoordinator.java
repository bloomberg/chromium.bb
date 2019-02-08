// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.support.v7.widget.GridLayoutManager;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase;

/**
 * Coordinator for showing a grid of tabs.
 */
public class TabGridCoordinator implements Destroyable {
    private static final int GRID_LAYOUT_SPAN_COUNT = 2;

    private final SimpleRecyclerViewMcpBase<PropertyModel, TabGridViewHolder, PropertyKey>
            mModelChangeProcessor;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;

    TabGridCoordinator(Context context, ToolbarManager toolbarManager,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            CompositorViewHolder compositorViewHolder) {
        TabListModel tabListModel = new TabListModel();
        mModelChangeProcessor = new SimpleRecyclerViewMcpBase<>(
                null, TabGridViewBinder::onBindViewHolder, tabListModel);
        RecyclerViewAdapter<TabGridViewHolder, PropertyKey> adapter =
                new RecyclerViewAdapter<>(mModelChangeProcessor, TabGridViewHolder::create);

        LayoutInflater.from(context).inflate(
                R.layout.tab_grid_recycler_view_layout, compositorViewHolder, true);
        mRecyclerView = compositorViewHolder.findViewById(R.id.tab_grid_view);
        mRecyclerView.setAdapter(adapter);
        mRecyclerView.setLayoutManager(new GridLayoutManager(context, GRID_LAYOUT_SPAN_COUNT));
        mRecyclerView.setHasFixedSize(true);

        mMediator = new TabListMediator(
                tabListModel, context, toolbarManager, tabModelSelector, tabContentManager);
    }

    /**
     * @return The container {@link android.support.v7.widget.RecyclerView} that is showing the tab
     *         grid.
     */
    public TabListRecyclerView getContainerView() {
        return mRecyclerView;
    }

    /**
     * Reset the tab grid with the given {@link TabModel}. Can be null.
     * @param tabModel The current {@link TabModel} to show the tabs for in the grid.
     */
    public void resetWithTabModel(TabModel tabModel) {
        mMediator.resetWithTabModel(tabModel);
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mMediator.destroy();
    }
}