// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.NavigationHandle;
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
    private boolean mShownIPH;

    /**
     * An interface to get the thumbnails to be shown inside the tab grid cards.
     */
    public interface ThumbnailProvider {
        void getTabThumbnailWithCallback(Tab tab, Callback<Bitmap> callback, boolean forceUpdate);
    }

    /**
     * An interface to get the title to be used for a tab.
     */
    public interface TitleProvider { String getTitle(Tab tab); }

    /**
     * The object to set to TabProperties.THUMBNAIL_FETCHER for the TabGridViewBinder to obtain
     * the thumbnail asynchronously.
     */
    static class ThumbnailFetcher {
        private ThumbnailProvider mThumbnailProvider;
        private Tab mTab;
        private boolean mForceUpdate;

        ThumbnailFetcher(ThumbnailProvider provider, Tab tab, boolean forceUpdate) {
            mThumbnailProvider = provider;
            mTab = tab;
            mForceUpdate = forceUpdate;
        }

        void fetch(Callback<Bitmap> callback) {
            mThumbnailProvider.getTabThumbnailWithCallback(mTab, callback, mForceUpdate);
        }
    }

    /**
     * An interface to show IPH for a tab.
     */
    public interface IphProvider { void showIPH(View anchor); }

    private final IphProvider mIphProvider = new IphProvider() {
        private static final int IPH_DELAY_MS = 1000;

        @Override
        public void showIPH(View anchor) {
            if (mShownIPH) return;
            mShownIPH = true;

            new Handler().postDelayed(
                    ()
                            -> TabGroupUtils.maybeShowIPH(
                                    FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
                                    anchor),
                    IPH_DELAY_MS);
        }
    };

    /**
     * An interface to get the onClickListener for "Create group" button.
     */
    public interface CreateGroupButtonProvider {
        /**
         * @return {@link TabActionListener} to create tab group. If the given {@link Tab} is not
         * able to create group, return null;
         */
        @Nullable
        TabActionListener getCreateGroupButtonOnClickListener(Tab tab);
    }

    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final ThumbnailProvider mThumbnailProvider;
    private final TabActionListener mTabClosedListener;
    private final TitleProvider mTitleProvider;
    private final CreateGroupButtonProvider mCreateGroupButtonProvider;
    private final String mComponentName;
    private boolean mCloseAllRelatedTabs;
    private ComponentCallbacks mComponentCallbacks;

    private final TabActionListener mTabSelectedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            Tab currentTab = mTabModelSelector.getCurrentTab();

            int newIndex =
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tabId);
            mTabModelSelector.getCurrentModel().setIndex(newIndex, TabSelectionType.FROM_USER);

            Tab newlySelectedTab = mTabModelSelector.getCurrentTab();

            if (currentTab == newlySelectedTab) {
                RecordUserAction.record("MobileTabReturnedToCurrentTab." + mComponentName);
            } else {
                recordTabOffsetOfSwitch(currentTab, newlySelectedTab);
                RecordUserAction.record("MobileTabSwitched." + mComponentName);
            }
        }
        private void recordTabOffsetOfSwitch(Tab fromTab, Tab toTab) {
            assert fromTab != toTab;

            int fromIndex = mTabModelSelector.getTabModelFilterProvider()
                                    .getCurrentTabModelFilter()
                                    .indexOf(fromTab);
            int toIndex = mTabModelSelector.getTabModelFilterProvider()
                                  .getCurrentTabModelFilter()
                                  .indexOf(toTab);

            if (fromIndex == toIndex) {
                fromIndex = TabModelUtils.getTabIndexById(
                        mTabModelSelector.getCurrentModel(), fromTab.getId());
                toIndex = TabModelUtils.getTabIndexById(
                        mTabModelSelector.getCurrentModel(), toTab.getId());
            }

            RecordHistogram.recordSparseHistogram(
                    "Tabs.TabOffsetOfSwitch." + mComponentName, fromIndex - toIndex);
        }
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;
            if (navigationHandle.isSameDocument() || !navigationHandle.isInMainFrame()) return;
            if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(mModel.indexFromId(tab.getId()))
                    .set(TabProperties.FAVICON,
                            mTabListFaviconProvider.getDefaultFaviconDrawable());
        }

        @Override
        public void onTitleUpdated(Tab updatedTab) {
            int index = mModel.indexFromId(updatedTab.getId());
            if (index == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(index).set(TabProperties.TITLE, mTitleProvider.getTitle(updatedTab));
        }

        @Override
        public void onFaviconUpdated(Tab updatedTab, Bitmap icon) {
            int index = mModel.indexFromId(updatedTab.getId());
            if (index == TabModel.INVALID_TAB_INDEX) return;
            Drawable drawable = mTabListFaviconProvider.getFaviconForUrlSync(
                    updatedTab.getUrl(), updatedTab.isIncognito(), icon);
            mModel.get(index).set(TabProperties.FAVICON, drawable);
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
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                                                 the tabs concerned.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param titleProvider {@link TitleProvider} for a given tab's title to show.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param closeRelatedTabs Whether all related tabs should be closed in {@link
     *         TabProperties#TAB_CLOSED_LISTENER}.
     * @param createGroupButtonProvider {@link CreateGroupButtonProvider} to provide "Create group"
     *                                   button information. It's null when "Create group" is not
     *                                   possible.
     * @param componentName This is a unique string to identify different components.
     */
    public TabListMediator(TabListModel model, TabModelSelector tabModelSelector,
            @Nullable ThumbnailProvider thumbnailProvider, @Nullable TitleProvider titleProvider,
            TabListFaviconProvider tabListFaviconProvider, boolean closeRelatedTabs,
            @Nullable CreateGroupButtonProvider createGroupButtonProvider, String componentName) {
        mTabModelSelector = tabModelSelector;
        mThumbnailProvider = thumbnailProvider;
        mModel = model;
        mTabListFaviconProvider = tabListFaviconProvider;
        mComponentName = componentName;
        mTitleProvider = titleProvider != null ? titleProvider : Tab::getTitle;
        mCreateGroupButtonProvider = createGroupButtonProvider;
        mCloseAllRelatedTabs = closeRelatedTabs;

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

            @Override
            public void tabClosureUndone(Tab tab) {
                onTabAdded(tab, !mCloseAllRelatedTabs);
            }

            @Override
            public void didAddTab(Tab tab, int type) {
                onTabAdded(tab, !mCloseAllRelatedTabs);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                mModel.removeAt(mModel.indexFromId(tab.getId()));
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mTabClosedListener = new TabActionListener() {
            @Override
            public void run(int tabId) {
                RecordUserAction.record("MobileTabClosed." + mComponentName);
                if (mCloseAllRelatedTabs) {
                    List<Tab> related = getRelatedTabsForId(tabId);
                    if (related.size() > 1) {
                        mTabModelSelector.getCurrentModel().closeMultipleTabs(related, true);
                        return;
                    }
                }
                mTabModelSelector.getCurrentModel().closeTab(
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId), false,
                        false, true);
            }
        };
    }

    public void setCloseAllRelatedTabsForTest(boolean closeAllRelatedTabs) {
        mCloseAllRelatedTabs = closeAllRelatedTabs;
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    private void onTabAdded(Tab tab, boolean onlyShowRelatedTabs) {
        List<Tab> related = getRelatedTabsForId(tab.getId());
        int index;
        if (onlyShowRelatedTabs) {
            index = related.indexOf(tab);
            if (index == -1) return;
        } else {
            index = TabModelUtils.getTabIndexById(
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                    tab.getId());
            // TODO(wychen): the title (tab count in the group) is wrong when it's not the last
            //  tab added in the group.
            if (index == TabList.INVALID_TAB_INDEX) return;
        }
        addTabInfoToModel(tab, index, mTabModelSelector.getCurrentTab() == tab);
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     * @param tabs The list of tabs to be shown.
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
     * @return The callback that hosts the logic for swipe and drag related actions.
     */
    ItemTouchHelper.SimpleCallback getItemTouchHelperCallback(final float swipeToDismissThreshold) {
        return new ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.START | ItemTouchHelper.END) {
            @Override
            public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder,
                    RecyclerView.ViewHolder viewHolder1) {
                return false;
            }

            @Override
            public void onSwiped(RecyclerView.ViewHolder viewHolder, int i) {
                assert viewHolder instanceof TabGridViewHolder;

                mTabClosedListener.run(((TabGridViewHolder) viewHolder).getTabId());
                RecordUserAction.record("MobileStackViewSwipeCloseTab." + mComponentName);
            }

            @Override
            public void onChildDraw(Canvas c, RecyclerView recyclerView,
                    RecyclerView.ViewHolder viewHolder, float dX, float dY, int actionState,
                    boolean isCurrentlyActive) {
                super.onChildDraw(
                        c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
                if (actionState == ItemTouchHelper.ACTION_STATE_SWIPE) {
                    float alpha =
                            Math.max(0.2f, 1f - 0.8f * Math.abs(dX) / swipeToDismissThreshold);
                    int index = mModel.indexFromId(((TabGridViewHolder) viewHolder).getTabId());
                    if (index == -1) return;

                    mModel.get(index).set(TabProperties.ALPHA, alpha);
                }
            }

            @Override
            public float getSwipeThreshold(RecyclerView.ViewHolder viewHolder) {
                return swipeToDismissThreshold / viewHolder.itemView.getWidth();
            }
        };
    }

    void registerOrientationListener(GridLayoutManager manager) {
        // TODO(yuezhanggg): Try to dynamically determine span counts based on screen width,
        // minimum card width and padding.
        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                manager.setSpanCount(newConfig.orientation == Configuration.ORIENTATION_PORTRAIT
                                ? TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_PORTRAIT
                                : TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_LANDSCAPE);
            }

            @Override
            public void onLowMemory() {}
        };
        ContextUtils.getApplicationContext().registerComponentCallbacks(mComponentCallbacks);
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
        if (mComponentCallbacks != null) {
            ContextUtils.getApplicationContext().unregisterComponentCallbacks(mComponentCallbacks);
        }
    }

    private void addTabInfoToModel(final Tab tab, int index, boolean isSelected) {
        TabActionListener createGroupButtonOnClickListener = null;
        if (isSelected && mCreateGroupButtonProvider != null) {
            createGroupButtonOnClickListener =
                    mCreateGroupButtonProvider.getCreateGroupButtonOnClickListener(tab);
        }
        boolean showIPH = false;
        if (mCloseAllRelatedTabs && !mShownIPH) {
            showIPH = getRelatedTabsForId(tab.getId()).size() > 1;
        }

        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.TITLE, mTitleProvider.getTitle(tab))
                        .with(TabProperties.FAVICON,
                                mTabListFaviconProvider.getDefaultFaviconDrawable())
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(TabProperties.IPH_PROVIDER, showIPH ? mIphProvider : null)
                        .with(TabProperties.TAB_SELECTED_LISTENER, mTabSelectedListener)
                        .with(TabProperties.TAB_CLOSED_LISTENER, mTabClosedListener)
                        .with(TabProperties.CREATE_GROUP_LISTENER, createGroupButtonOnClickListener)
                        .with(TabProperties.ALPHA, 1f)
                        .build();

        if (index >= mModel.size()) {
            mModel.add(tabInfo);
        } else {
            mModel.add(index, tabInfo);
        }

        Callback<Drawable> faviconCallback = drawable -> {
            int modelIndex = mModel.indexFromId(tab.getId());
            if (modelIndex != Tab.INVALID_TAB_ID && drawable != null) {
                mModel.get(modelIndex).set(TabProperties.FAVICON, drawable);
            }
        };
        mTabListFaviconProvider.getFaviconForUrlAsync(
                tab.getUrl(), tab.isIncognito(), faviconCallback);

        if (mThumbnailProvider != null) {
            ThumbnailFetcher callback = new ThumbnailFetcher(mThumbnailProvider, tab, isSelected);
            tabInfo.set(TabProperties.THUMBNAIL_FETCHER, callback);
        }
        tab.addObserver(mTabObserver);
    }
}
