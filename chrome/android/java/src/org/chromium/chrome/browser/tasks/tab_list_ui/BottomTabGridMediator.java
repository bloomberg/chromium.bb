// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.view.View.OnClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

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
         * @param tabModel current {@link TabModel} instance.
         */
        void resetWithTabModel(TabModel tabModel);
    }

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mSheetObserver;
    private final PropertyModel mToolbarPropertyModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;

    BottomTabGridMediator(Context context, BottomSheetController bottomSheetController,
            ResetHandler resetHandler, PropertyModel toolbarPropertyModel,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mToolbarPropertyModel = toolbarPropertyModel;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;

        // TODO (ayman): Add instrumentation to observer calls.
        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                resetHandler.resetWithTabModel(null);
            }
        };

        // register for tab model
        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                updateBottomSheetTitle();
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateBottomSheetTitle();
            }
        };

        // setup toolbar property model
        setupToolbarClickHandlers();
        updateBottomSheetTitle();
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
        mTabModelObserver.destroy();
    }

    private void showTabGridInBottomSheet(BottomTabGridSheetContent sheetContent) {
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

    private void updateBottomSheetTitle() {
        int tabsCount = mTabModelSelector.getCurrentModel().getCount();
        mToolbarPropertyModel.set(BottomTabGridSheetToolbarProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount));
    }

    private void setupToolbarClickHandlers() {
        mToolbarPropertyModel.set(BottomTabGridSheetToolbarProperties.COLLAPSE_CLICK_LISTENER,
                getCollapseButtonClickListener());
        mToolbarPropertyModel.set(BottomTabGridSheetToolbarProperties.ADD_CLICK_LISTENER,
                getAddButtonClickListener());
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
