// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Mediator for business logic for the tab grid. This class should be initialized with a list of
 * tabs and a TabModel to observe for changes and should not have any logic around what the list
 * signifies.
 * TODO(yusufo): Move some of the logic here to a parent component to make the above true.
 */
class TabListMediator {
    /**
     * An interface to get the thumbnails to be shown inside the tab grid cards.
     */
    public interface ThumbnailProvider {
        void getTabThumbnailWithCallback(Tab tab, Callback<Bitmap> callback);
    }

    /**
     * The object to set to TabProperties.THUMBNAIL_FETCHER for the TabGridViewBinder to obtain
     * the thumbnail asynchronously.
     */
    static class ThumbnailFetcher {
        private ThumbnailProvider mThumbnailProvider;
        private Tab mTab;

        ThumbnailFetcher(ThumbnailProvider provider, Tab tab) {
            mThumbnailProvider = provider;
            mTab = tab;
        }

        void fetch(Callback<Bitmap> callback) {
            mThumbnailProvider.getTabThumbnailWithCallback(mTab, callback);
        }
    }
    private final int mFaviconSize;
    private final FaviconHelper mFaviconHelper;
    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabContentManager mTabContentManager;
    private final Profile mProfile;

    private final TabActionListener mTabSelectedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            int currentIndex = mTabModelSelector.getCurrentModel().index();
            int newIndex =
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tabId);
            mTabModelSelector.getCurrentModel().setIndex(newIndex, TabSelectionType.FROM_USER);

            if (newIndex == currentIndex) {
                RecordUserAction.record("MobileTabReturnedToCurrentTab");
            } else {
                RecordHistogram.recordSparseHistogram(
                        "Tabs.TabOffsetOfSwitch", currentIndex - newIndex);
            }
        }
    };

    private final TabActionListener mTabClosedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            mTabModelSelector.getCurrentModel().closeTab(
                    TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId), false,
                    false, true);
            RecordUserAction.record("MobileStackViewCloseTab");
        }
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab updatedTab) {
            int index = mModel.indexFromId(updatedTab.getId());
            if (index == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(index).set(TabProperties.TITLE, updatedTab.getTitle());
        }

        @Override
        public void onFaviconUpdated(Tab updatedTab, Bitmap icon) {
            int index = mModel.indexFromId(updatedTab.getId());
            if (index == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(index).set(TabProperties.FAVICON, icon);
        }
    };

    private final TabModelObserver mTabModelObserver;

    /**
     * Interface for implementing a {@link Runnable} that takes a tabId for a generic action.
     */
    public interface TabActionListener { void run(int tabId); }

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     * @param model The Model to keep state about a list of {@link Tab}s.
     * @param context The context to use for accessing {@link android.content.res.Resources}
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                                                 the tabs concerned.
     * @param tabContentManager {@link TabContentManager} to provide screenshot related details.
     */
    public TabListMediator(Profile profile, TabListModel model, Context context,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            FaviconHelper faviconHelper) {
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);

        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mModel = model;
        mFaviconHelper = faviconHelper;
        mProfile = profile;

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (tab.getId() == lastId) return;
                int oldIndex = mModel.indexFromId(lastId);
                if (oldIndex != TabModel.INVALID_TAB_INDEX) {
                    mModel.get(oldIndex).set(TabProperties.IS_SELECTED, false);
                }
                int newIndex = mModel.indexFromId(tab.getId());
                if (newIndex == TabModel.INVALID_TAB_INDEX) return;

                mModel.get(newIndex).set(TabProperties.IS_SELECTED, true);
            }

            // TODO(meiliang): should not use index from tabmodel.
            @Override
            public void tabClosureUndone(Tab tab) {
                int index = TabModelUtils.getTabIndexById(
                        mTabModelSelector.getCurrentModel(), tab.getId());
                addTabInfoToModel(tab, index, mTabModelSelector.getCurrentModel().index() == index);
            }

            @Override
            public void didAddTab(Tab tab, int type) {
                int index = TabModelUtils.getTabIndexById(
                        mTabModelSelector.getCurrentModel(), tab.getId());
                if (index == TabModel.INVALID_TAB_INDEX) return;

                addTabInfoToModel(tab, index, mTabModelSelector.getCurrentModel().index() == index);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                mModel.removeAt(mModel.indexFromId(tab.getId()));
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     * @param tabs
     */
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mModel.set(new ArrayList<>());
        if (tabs == null) {
            return;
        }
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;

        for (int i = 0; i < tabs.size(); i++) {
            addTabInfoToModel(tabs.get(i), i, tabs.get(i).getId() == currentTab.getId());
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel != null) {
            for (int i = 0; i < tabModel.getCount(); i++) {
                tabModel.getTabAt(i).removeObserver(mTabObserver);
            }
        }
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
    }

    private void addTabInfoToModel(final Tab tab, int index, boolean isSelected) {
        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.TITLE, tab.getTitle())
                        .with(TabProperties.FAVICON, TabFavicon.getBitmap(tab))
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(TabProperties.TAB_SELECTED_LISTENER, mTabSelectedListener)
                        .with(TabProperties.TAB_CLOSED_LISTENER, mTabClosedListener)
                        .build();
        if (index >= mModel.size()) {
            mModel.add(tabInfo);
        } else {
            mModel.add(index, tabInfo);
        }
        mFaviconHelper.getLocalFaviconImageForURL(
                mProfile, tab.getUrl(), mFaviconSize, (image, iconUrl) -> {
                    if (mModel.indexFromId(tab.getId()) == Tab.INVALID_TAB_ID) return;
                    mModel.get(mModel.indexFromId(tab.getId())).set(TabProperties.FAVICON, image);
                });
        ThumbnailFetcher callback =
                new ThumbnailFetcher(mTabContentManager::getTabThumbnailWithCallback, tab);
        tabInfo.set(TabProperties.THUMBNAIL_FETCHER, callback);
        tab.addObserver(mTabObserver);
    }
}
