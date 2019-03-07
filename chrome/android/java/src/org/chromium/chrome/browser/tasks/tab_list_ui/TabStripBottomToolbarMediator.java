// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the TabStripBottomToolbar. Responsible for managing the
 * internal state of the component.
 */
public class TabStripBottomToolbarMediator implements Destroyable {
    /**
     * Defines an interface for a {@link TabStripBottomToolbarMediator} reset event
     * handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabStripBottomToolbarMediator}
         * when the bottom sheet is collaped.
         *
         * @param tabModel current {@link TabModel} instance.
         */
        void resetStripWithTabModel(TabModel tabModel);

        /**
         * Handles a reset event originated from {@link TabStripBottomToolbarMediator}
         * when the bottom sheet is expanded and the component.
         *
         * @param tabModel current {@link TabModel} instance.
         */
        void resetSheetWithTabModel(TabModel tabModel);
    }

    private final PropertyModel mToolbarPropertyModel;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final ResetHandler mResetHandler;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;

    TabStripBottomToolbarMediator(ResetHandler resetHandler, PropertyModel toolbarPropertyModel,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager) {
        mResetHandler = resetHandler;
        mToolbarPropertyModel = toolbarPropertyModel;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;

        // register for tab model
        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (TabModelUtils.getTabById(tabModelSelector.getCurrentModel(), lastId) != null)
                    return;

                mResetHandler.resetStripWithTabModel(tabModelSelector.getCurrentModel());
            }
        };

        setupToolbarClickHandlers();
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE, true);
        mResetHandler.resetStripWithTabModel(tabModelSelector.getCurrentModel());
    }

    private void setupToolbarClickHandlers() {
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.EXPAND_CLICK_LISTENER, view -> {
            mResetHandler.resetSheetWithTabModel(mTabModelSelector.getCurrentModel());
        });
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.ADD_CLICK_LISTENER, view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL), TabLaunchType.FROM_LINK,
                            currentTab);
        });
    }

    @Override
    public void destroy() {
        mTabModelObserver.destroy();
    }
}
