// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.LinearLayoutManager;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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

    private static final int GRID_LAYOUT_SPAN_COUNT = 2;
    private final SimpleRecyclerViewMcpBase mModelChangeProcessor;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;

    TabListCoordinator(@TabListMode int mode, Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, @NonNull ViewGroup parentView,
            boolean attachToParent) {
        TabListModel tabListModel = new TabListModel();

        RecyclerViewAdapter adapter;
        if (mode == TabListMode.GRID) {
            SimpleRecyclerViewMcpBase<PropertyModel, TabGridViewHolder, PropertyKey> mcp =
                    new SimpleRecyclerViewMcpBase<>(
                            null, TabGridViewBinder::onBindViewHolder, tabListModel);
            adapter = new RecyclerViewAdapter<>(mcp, TabGridViewHolder::create);
            mModelChangeProcessor = mcp;
        } else if (mode == TabListMode.STRIP) {
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
                    R.layout.tab_list_recycler_view_layout, parentView, false);
        } else {
            LayoutInflater.from(context).inflate(
                    R.layout.tab_list_recycler_view_layout, parentView, true);
            mRecyclerView = parentView.findViewById(R.id.tab_list_view);
        }

        mRecyclerView.setAdapter(adapter);

        if (mode == TabListMode.GRID) {
            mRecyclerView.setLayoutManager(new GridLayoutManager(context, GRID_LAYOUT_SPAN_COUNT));
        } else if (mode == TabListMode.STRIP) {
            mRecyclerView.setLayoutManager(
                    new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        }

        mRecyclerView.setHasFixedSize(true);

        mMediator = new TabListMediator(Profile.getLastUsedProfile(), tabListModel, context,
                tabModelSelector, tabContentManager, new FaviconHelper());
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
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mMediator.destroy();
    }
}
