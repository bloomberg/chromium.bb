// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

/**
 * A mediator for the BottomTabGrid component, respoonsible for communicating with
 * the components' coordinator as well as managing the state of the bottom sheet.
 */
class BottomTabGridMediator {
    /**
     * Defines an interface for a {@link BottomTabGridMediator} reset event handler.
     */
    interface ResetHandler {
        /**
         * Handles reset event originating from {@link BottomTabGridMediator}.
         * @param tabModel current {@link TabModel} instance.
         */
        void resetWithTabModel(TabModel tabModel);
    }

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mSheetObserver;

    BottomTabGridMediator(BottomSheetController bottomSheetController, ResetHandler resetHandler) {
        mBottomSheetController = bottomSheetController;

        // TODO (ayman): Add instrumentation to observer calls.
        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                resetHandler.resetWithTabModel(null);
            }
        };
    }

    /**
     * Handles communication with the bottom sheet based on the content provided.
     * @param sheetContent The {@link BottomTabGridSheetContent} to populate the bottom sheet with.
     *         When set to null, the bottom sheet will be hidden.
     */
    void onReset(BottomTabGridSheetContent sheetContent) {
        if (sheetContent == null) {
            removeSuggestionsFromSheet(sheetContent);
        } else {
            showTabGridInBottomSheet(sheetContent);
        }
    }

    private void showTabGridInBottomSheet(BottomTabGridSheetContent sheetContent) {
        mBottomSheetController.getBottomSheet().addObserver(mSheetObserver);
        mBottomSheetController.requestShowContent(sheetContent, true);
        mBottomSheetController.expandSheet();
    }

    private void removeSuggestionsFromSheet(BottomTabGridSheetContent sheetContent) {
        mBottomSheetController.hideContent(sheetContent, true);
        mBottomSheetController.getBottomSheet().removeObserver(mSheetObserver);
    }
}
