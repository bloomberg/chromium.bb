// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Handles context menu creation for native pages on touchless devices. Utilizes functionality in
 * ContextMenuManager and Delegate to select items to show and lookup their labels.
 */
public class TouchlessContextMenuManager extends ContextMenuManager {
    private PropertyModel mTouchlessMenuModel;
    private ModalDialogManager mModalDialogManager;

    public TouchlessContextMenuManager(NativePageNavigationDelegate navigationDelegate,
            TouchEnabledDelegate touchEnabledDelegate, Runnable closeContextMenuCallback,
            String userActionPrefix) {
        super(navigationDelegate, touchEnabledDelegate, closeContextMenuCallback, userActionPrefix);
    }

    /**
     * Creates PropertyModel for touchless context menu and displays it through modalDialogManager.
     *
     * @param modalDialogManager The ModalDialogManager to display context menu.
     * @param context The Context used for loading resources (strings, drawables).
     * @param delegate Delegate defines filter for displayed menu items and behavior for selects
     *                 item.
     */
    public void showTouchlessContextMenu(
            ModalDialogManager modalDialogManager, Context context, Delegate delegate) {
        ArrayList<PropertyModel> menuItems = new ArrayList();
        for (@ContextMenuItemId int itemId = 0; itemId < ContextMenuItemId.NUM_ENTRIES; itemId++) {
            if (!shouldShowItem(itemId, delegate)) continue;

            // Each menu item is assigned its own instance of TouchlessItemClickListener where
            // itemId of this item is maintained.
            PropertyModel menuItem = buildMenuItem(context, getResourceIdForMenuItem(itemId),
                    new TouchlessItemClickListener(delegate, itemId));
            menuItems.add(menuItem);
        }
        if (menuItems.size() == 0) return;

        PropertyModel[] menuItemsArray = new PropertyModel[menuItems.size()];
        menuItemsArray = menuItems.toArray(menuItemsArray);
        mTouchlessMenuModel = buildMenuModel(context, menuItemsArray);
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.showDialog(mTouchlessMenuModel, ModalDialogManager.ModalDialogType.APP);

        notifyContextMenuShown(delegate);
    }

    @Override
    protected boolean shouldShowItem(@ContextMenuItemId int itemId, Delegate delegate) {
        // Here we filter out any item IDs that don't make sense in touchless.
        switch (itemId) {
            case ContextMenuItemId.REMOVE:
                // fall through
            case ContextMenuItemId.LEARN_MORE:
                return super.shouldShowItem(itemId, delegate);
            case ContextMenuItemId.SAVE_FOR_OFFLINE:
                // fall through
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                // fall through
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                // fall through
            case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                return false;
        }

        assert false : "Encountered unexpected touchless context menu item type";
        return false;
    }

    /**
     * Builds PropertyModel for individual menu item. String for item's label is loaded from
     * resources by PropertyModel.Builder.
     */
    private PropertyModel buildMenuItem(
            Context context, @StringRes int itemTextId, OnClickListener listener) {
        return new PropertyModel.Builder(DialogListItemProperties.ALL_KEYS)
                .with(DialogListItemProperties.TEXT, context.getResources(), itemTextId)
                .with(DialogListItemProperties.CLICK_LISTENER, listener)
                .build();
    }

    /** Builds PropertyModel for context menu from list of PropertyModels for individual items. */
    private PropertyModel buildMenuModel(Context context, PropertyModel[] menuItems) {
        return new PropertyModel.Builder(TouchlessDialogProperties.ALL_DIALOG_KEYS)
                .with(ModalDialogProperties.CONTROLLER,
                        new ModalDialogProperties.Controller() {
                            @Override
                            public void onClick(PropertyModel model, int buttonType) {}

                            @Override
                            public void onDismiss(PropertyModel model, int dismissalCause) {}
                        })
                .with(TouchlessDialogProperties.ALT_NAME, "")
                .with(TouchlessDialogProperties.SELECT_NAME, context.getResources(),
                        org.chromium.chrome.R.string.select)
                .with(TouchlessDialogProperties.CANCEL_NAME, context.getResources(),
                        org.chromium.chrome.R.string.cancel)
                .with(TouchlessDialogProperties.CANCEL_ACTION, (v) -> closeTouchlessContextMenu())
                .with(TouchlessDialogProperties.LIST_MODELS, menuItems)
                .with(TouchlessDialogProperties.PRIORITY, TouchlessDialogProperties.Priority.HIGH)
                .build();
    }

    private void closeTouchlessContextMenu() {
        mModalDialogManager.dismissDialog(mTouchlessMenuModel, 0);
    }

    private class TouchlessItemClickListener implements OnClickListener {
        private final Delegate mDelegate;
        private final @ContextMenuItemId int mItemId;

        public TouchlessItemClickListener(Delegate delegate, @ContextMenuItemId int itemId) {
            mDelegate = delegate;
            mItemId = itemId;
        }

        @Override
        public void onClick(View view) {
            closeTouchlessContextMenu();
            handleMenuItemClick(mItemId, mDelegate);
        }
    }
}
