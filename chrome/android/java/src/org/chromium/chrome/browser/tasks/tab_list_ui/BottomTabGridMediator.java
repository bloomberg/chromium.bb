// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View.OnClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the BottomTabGrid component, respoonsible for communicating
 * with the components' coordinator as well as managing the state of the bottom
 * sheet.
 */
class BottomTabGridMediator implements Destroyable {
    /**
     * Defines an interface for a {@link BottomTabGridMediator} reset event handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link BottomTabGridMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);
    }

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mSheetObserver;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;

    BottomTabGridMediator(Context context, BottomSheetController bottomSheetController,
            ResetHandler resetHandler, PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;

        // TODO (ayman): Add instrumentation to observer calls.
        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                resetHandler.resetWithListOfTabs(null);
            }
        };

        // register for tab model
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                updateBottomSheetTitleAndMargin();
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateBottomSheetTitleAndMargin();
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_USER) resetHandler.resetWithListOfTabs(null);
            }
        };
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        // setup toolbar property model
        setupToolbarClickHandlers();
        updateBottomSheetTitleAndMargin();
    }

    /**
     * Handles communication with the bottom sheet based on the content provided.
     *
     * @param sheetContent The {@link BottomTabGridSheetContent} to populate the
     *                     bottom sheet with. When set to null, the bottom sheet
     *                     will be hidden.
     */
    void onReset(BottomTabGridSheetContent sheetContent) {
        if (sheetContent == null) {
            hideBottomTabGridSheet();
        } else {
            showTabGridInBottomSheet(sheetContent);
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
    }

    private void showTabGridInBottomSheet(BottomTabGridSheetContent sheetContent) {
        updateBottomSheetTitleAndMargin();
        mBottomSheetController.getBottomSheet().addObserver(mSheetObserver);
        mBottomSheetController.requestShowContent(sheetContent, true);
        mBottomSheetController.expandSheet();
    }

    private void hideBottomTabGridSheet() {
        mBottomSheetController.hideContent(getCurrentSheetContent(), true);
        mBottomSheetController.getBottomSheet().removeObserver(mSheetObserver);
    }

    private BottomSheetContent getCurrentSheetContent() {
        return mBottomSheetController.getBottomSheet() != null
                ? mBottomSheetController.getBottomSheet().getCurrentSheetContent()
                : null;
    }

    private void updateBottomSheetTitleAndMargin() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        int tabsCount = mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .getRelatedTabList(currentTab.getId())
                                .size();
        mModel.set(BottomTabGridSheetProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount));
        mModel.set(BottomTabGridSheetProperties.CONTENT_TOP_MARGIN,
                (int) mContext.getResources().getDimension(R.dimen.control_container_height));
    }

    private void setupToolbarClickHandlers() {
        mModel.set(BottomTabGridSheetProperties.COLLAPSE_CLICK_LISTENER,
                getCollapseButtonClickListener());
        mModel.set(BottomTabGridSheetProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private OnClickListener getCollapseButtonClickListener() {
        return view -> {
            hideBottomTabGridSheet();
        };
    }

    private OnClickListener getAddButtonClickListener() {
        return view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL), TabLaunchType.FROM_LINK,
                            currentTab);
        };
    }
}
