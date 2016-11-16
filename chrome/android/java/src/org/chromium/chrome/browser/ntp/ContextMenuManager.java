// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.annotation.IntDef;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetsConfig;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Takes care of creating, closing a context menu and triaging the item clicks.
 */
public class ContextMenuManager {
    @IntDef({ID_OPEN_IN_NEW_WINDOW, ID_OPEN_IN_NEW_TAB, ID_OPEN_IN_INCOGNITO_TAB, ID_REMOVE,
            ID_SAVE_FOR_OFFLINE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuItemId {}
    public static final int ID_OPEN_IN_NEW_WINDOW = 0;
    public static final int ID_OPEN_IN_NEW_TAB = 1;
    public static final int ID_OPEN_IN_INCOGNITO_TAB = 2;
    public static final int ID_REMOVE = 3;
    public static final int ID_SAVE_FOR_OFFLINE = 4;

    private final NewTabPageManager mManager;
    private final ChromeActivity mActivity;
    private final TouchDisableableView mOuterView;

    /** Defines callback to configure the context menu and respond to user interaction. */
    public interface Delegate {
        /** Opens the current item the way specified by {@code windowDisposition}. */
        void openItem(int windowDisposition);

        /** Remove the current item. */
        void removeItem();

        /** @return whether the current item can be saved offline. */
        String getUrl();

        /** @return IDs of the menu items that are supported. */
        Set<Integer> getSupportedMenuItems();
    }

    /** Interface for a view that can be set to stop responding to touches. */
    public interface TouchDisableableView { void setTouchEnabled(boolean enabled); }

    public ContextMenuManager(NewTabPageManager newTabPageManager, ChromeActivity activity,
            TouchDisableableView outerView) {
        mManager = newTabPageManager;
        mActivity = activity;
        mOuterView = outerView;
    }

    /**
     * Populates the context menu.
     * @param menu The menu to populate.
     * @param associatedView The view that requested a context menu.
     * @param delegate Delegate that defines the configuration of the menu and what to do when items
     *                 are tapped.
     */
    public void createContextMenu(ContextMenu menu, View associatedView, Delegate delegate) {
        OnMenuItemClickListener listener = new ItemClickListener(delegate);
        Set<Integer> supportedItems = delegate.getSupportedMenuItems();
        boolean hasItems = false;

        if (shouldShowItem(ID_OPEN_IN_NEW_WINDOW, delegate, supportedItems)) {
            hasItems = true;
            addContextMenuItem(menu, ID_OPEN_IN_NEW_WINDOW,
                    R.string.contextmenu_open_in_other_window, listener);
        }

        if (shouldShowItem(ID_OPEN_IN_NEW_TAB, delegate, supportedItems)) {
            hasItems = true;
            addContextMenuItem(
                    menu, ID_OPEN_IN_NEW_TAB, R.string.contextmenu_open_in_new_tab, listener);
        }

        if (shouldShowItem(ID_OPEN_IN_INCOGNITO_TAB, delegate, supportedItems)) {
            hasItems = true;
            addContextMenuItem(menu, ID_OPEN_IN_INCOGNITO_TAB,
                    R.string.contextmenu_open_in_incognito_tab, listener);
        }

        if (shouldShowItem(ID_SAVE_FOR_OFFLINE, delegate, supportedItems)) {
            hasItems = true;
            addContextMenuItem(menu, ID_SAVE_FOR_OFFLINE, R.string.contextmenu_save_link, listener);
        }

        if (shouldShowItem(ID_REMOVE, delegate, supportedItems)) {
            hasItems = true;
            addContextMenuItem(menu, ID_REMOVE, R.string.remove, listener);
        }

        // No item added. We won't show the menu, so we can skip the rest.
        if (!hasItems) return;

        associatedView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                closeContextMenu();
                view.removeOnAttachStateChangeListener(this);
            }
        });

        // Disable touch events on the outer view while the context menu is open. This is to
        // prevent the user long pressing to get the context menu then on the same press scrolling
        // or swiping to dismiss an item (eg. https://crbug.com/638854, https://crbug.com/638555,
        // https://crbug.com/636296)
        mOuterView.setTouchEnabled(false);

        mActivity.addContextMenuCloseCallback(new Callback<Menu>() {
            @Override
            public void onResult(Menu result) {
                mOuterView.setTouchEnabled(true);
                mActivity.removeContextMenuCloseCallback(this);
            }
        });
    }

    /** Closes the context menu, if open. */
    public void closeContextMenu() {
        mActivity.closeContextMenu();
    }

    private boolean shouldShowItem(
            @ContextMenuItemId int itemId, Delegate delegate, Set<Integer> supportedItems) {
        if (!supportedItems.contains(itemId)) return false;

        switch (itemId) {
            case ID_OPEN_IN_NEW_WINDOW:
                return mManager.isOpenInNewWindowEnabled();
            case ID_OPEN_IN_NEW_TAB:
                return true;
            case ID_OPEN_IN_INCOGNITO_TAB:
                return mManager.isOpenInIncognitoEnabled();
            case ID_SAVE_FOR_OFFLINE: {
                if (!SnippetsConfig.isSaveToOfflineEnabled()) return false;
                String itemUrl = delegate.getUrl();
                return itemUrl != null && OfflinePageBridge.canSavePage(itemUrl);
            }
            case ID_REMOVE:
                return true;

            default:
                assert false;
                return false;
        }
    }

    /**
     * Convenience method to reduce multi-line function call to single line.
     */
    private void addContextMenuItem(ContextMenu menu, @ContextMenuItemId int id, int resourceId,
            OnMenuItemClickListener listener) {
        menu.add(Menu.NONE, id, Menu.NONE, resourceId).setOnMenuItemClickListener(listener);
    }

    private static class ItemClickListener implements OnMenuItemClickListener {
        private final Delegate mDelegate;

        ItemClickListener(Delegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            switch (item.getItemId()) {
                case ID_OPEN_IN_NEW_WINDOW:
                    mDelegate.openItem(WindowOpenDisposition.NEW_WINDOW);
                    return true;
                case ID_OPEN_IN_NEW_TAB:
                    mDelegate.openItem(WindowOpenDisposition.NEW_FOREGROUND_TAB);
                    return true;
                case ID_OPEN_IN_INCOGNITO_TAB:
                    mDelegate.openItem(WindowOpenDisposition.OFF_THE_RECORD);
                    return true;
                case ID_SAVE_FOR_OFFLINE:
                    mDelegate.openItem(WindowOpenDisposition.SAVE_TO_DISK);
                    return true;
                case ID_REMOVE:
                    mDelegate.removeItem();
                    return true;
                default:
                    return false;
            }
        }
    }
}
