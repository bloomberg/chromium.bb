// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGridDialog component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for dialog show/hide.
 */
public class TabGridDialogMediator {
    /**
     * Defines an interface for a {@link TabGridDialogMediator} reset event handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGridDialogMediator} and {@link
         * GridTabSwitcherMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);
    }

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;
    private final TabGridDialogMediator.ResetHandler mDialogResetHandler;
    private final GridTabSwitcherMediator.ResetHandler mGridTabSwitcherResetHandler;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;

    TabGridDialogMediator(Context context, TabGridDialogMediator.ResetHandler dialogResetHandler,
            PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            GridTabSwitcherMediator.ResetHandler gridTabSwitcherResetHandler) {
        mContext = context;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mDialogResetHandler = dialogResetHandler;
        mGridTabSwitcherResetHandler = gridTabSwitcherResetHandler;

        // Register for tab model.
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateDialog();
                updateGridTabSwitcher();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateDialog();
                updateGridTabSwitcher();
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_USER)
                    mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                updateDialog();
                updateGridTabSwitcher();
                List<Tab> relatedTabs = getRelatedTabs(tab.getId());
                // If current tab is closed and tab group is not empty, hand over ID of the next
                // tab in the group to mCurrentTabId.
                if (relatedTabs.size() == 0) return;
                if (tab.getId() == mCurrentTabId) {
                    mCurrentTabId = relatedTabs.get(0).getId();
                }
            }
        };
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        // Setup toolbar property model.
        setupToolbarClickHandlers();

        // Setup ScrimView observer.
        setupScrimViewObserver();
    }

    void onReset(Integer tabId) {
        if (tabId != null) {
            mCurrentTabId = tabId;
            updateDialog();
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        } else {
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
    }

    private void updateGridTabSwitcher() {
        mGridTabSwitcherResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());
    }

    private void updateDialog() {
        if (mCurrentTabId == Tab.INVALID_TAB_ID) return;
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int tabsCount = relatedTabs.size();
        if (tabsCount == 0) {
            mDialogResetHandler.resetWithListOfTabs(null);
            return;
        }
        mModel.set(TabGridSheetProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        org.chromium.chrome.R.plurals.bottom_tab_grid_title_placeholder, tabsCount,
                        tabsCount));
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridSheetProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridSheetProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private void setupScrimViewObserver() {
        ScrimView.ScrimObserver scrimObserver = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
            }
            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridSheetProperties.SCRIMVIEW_OBSERVER, scrimObserver);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
            List<Tab> relatedTabs = getRelatedTabs(currentTab.getId());

            assert relatedTabs.size() > 0;

            Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_CHROME_UI, parentTabToAttach);
        };
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }
}
