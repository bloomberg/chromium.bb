// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_VISIBLE;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for business logic for the tab grid. This class should be initialized with a list of
 * tabs and a TabModel to observe for changes and should not have any logic around what the list
 * signifies.
 * TODO(yusufo): Move some of the logic here to a parent component to make the above true.
 */
class TabListMediator {
    private final int mFaviconSize;
    private final FaviconHelper mFaviconHelper = new FaviconHelper();
    private final TabListModel mModel;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabContentManager mTabContentManager;

    private final TabActionListener mTabSelectedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            mTabModelSelector.getCurrentModel().setIndex(
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tabId),
                    TabSelectionType.FROM_USER);
            if (mTabModelSelector.getCurrentTabId() == tabId) {
                mContainerViewModel.set(IS_VISIBLE, false);
            }
        }
    };

    private final TabActionListener mTabClosedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            mTabModelSelector.getCurrentModel().closeTab(
                    TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
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

    private final TabModelSelectorTabModelObserver mTabModelObserver;

    /**
     * Interface for implementing a {@link Runnable} that takes a tabId for a generic action.
     */
    public interface TabActionListener { void run(int tabId); }

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     * @param containerViewModel The {@link PropertyModel} to keep state about the container view.
     * @param model The Model to keep state about a list of {@link Tab}s.
     * @param context The context to use for accessing {@link android.content.res.Resources}
     * @param toolbarManager {@link ToolbarManager} to send any signals about overriding behavior.
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                                                 the tabs concerned.
     * @param tabContentManager {@link TabContentManager} to provide screenshot related details.
     */
    public TabListMediator(PropertyModel containerViewModel, TabListModel model, Context context,
            ToolbarManager toolbarManager, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager) {
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);

        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mModel = model;

        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (tab.getId() == lastId) return;
                if (mModel.indexFromId(lastId) != TabModel.INVALID_TAB_INDEX) {
                    mModel.get(mModel.indexFromId(lastId)).set(TabProperties.IS_SELECTED, false);
                }
                mModel.get(mModel.indexFromId(tab.getId())).set(TabProperties.IS_SELECTED, true);
                mContainerViewModel.set(IS_VISIBLE, false);
            }

            @Override
            public void didAddTab(Tab tab, int type) {
                addTabInfoToModel(tab, false);
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                mModel.removeAt(mModel.indexFromId(tabId));
            }
        };

        // TODO(yusufo): Move the calls below to a parent component, and change resetWithTabModel to
        // take a List of Tabs, so that this class stays oblivious to switcher button related
        // logistics. Also lazily initialize TabGridComponent on button click once there are two
        // components.
        mContainerViewModel = containerViewModel;
        toolbarManager.overrideTabSwitcherBehavior(view
                -> mContainerViewModel.set(IS_VISIBLE, !mContainerViewModel.get(IS_VISIBLE)),
                null);

        resetWithTabModel(mTabModelSelector.getCurrentModel());
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     */
    public void resetWithTabModel(TabModel tabModel) {
        mContainerViewModel.set(IS_INCOGNITO, tabModel.isIncognito());

        int selectedIndex = tabModel.index();
        for (int i = 0; i < tabModel.getCount(); i++) {
            addTabInfoToModel(tabModel.getTabAt(i), i == selectedIndex);
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
        mTabModelObserver.destroy();
    }

    private void addTabInfoToModel(final Tab tab, boolean isSelected) {
        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.TITLE, tab.getTitle())
                        .with(TabProperties.FAVICON, tab.getFavicon())
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(TabProperties.TAB_SELECTED_LISTENER, mTabSelectedListener)
                        .with(TabProperties.TAB_CLOSED_LISTENER, mTabClosedListener)
                        .build();
        mModel.add(tabInfo);
        mFaviconHelper.getLocalFaviconImageForURL(Profile.getLastUsedProfile().getOriginalProfile(),
                tab.getUrl(), mFaviconSize,
                (image, iconUrl)
                        -> mModel.get(mModel.indexFromId(tab.getId()))
                                   .set(TabProperties.FAVICON, image));
        tab.addObserver(mTabObserver);
    }
}
