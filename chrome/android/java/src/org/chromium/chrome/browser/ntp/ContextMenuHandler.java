// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.annotation.IntDef;
import android.support.v4.view.ViewCompat;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Takes care of creating a context menu and triaging the time clicks.
 */
public class ContextMenuHandler implements OnMenuItemClickListener {
    @IntDef({ID_OPEN_IN_NEW_WINDOW, ID_OPEN_IN_NEW_TAB, ID_OPEN_IN_INCOGNITO_TAB, ID_REMOVE,
            ID_SAVE_FOR_OFFLINE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContextMenuItemId {}
    static final int ID_OPEN_IN_NEW_WINDOW = 0;
    static final int ID_OPEN_IN_NEW_TAB = 1;
    static final int ID_OPEN_IN_INCOGNITO_TAB = 2;
    static final int ID_REMOVE = 3;
    static final int ID_SAVE_FOR_OFFLINE = 4;

    private final NewTabPageManager mManager;
    private final Delegate mDelegate;
    private final TouchDisableableView mOuterView;

    /** Defines callback to configure the context menu and respond to user interaction. */
    public interface Delegate {
        /** Opens the current item the way specified by {@code windowDisposition}. */
        void openItem(int windowDisposition);

        /** Removed the current item. */
        void removeItem();

        /** @return whether the current item can be saved offline. */
        boolean canBeSavedOffline();
    }

    /** Interface for a view that can be set to stop responding to touches. */
    public interface TouchDisableableView {
        void setTouchEnabled(boolean enabled);
        View asView();
    }

    public ContextMenuHandler(NewTabPageManager newTabPageManager, TouchDisableableView outerView,
            Delegate delegate) {
        assert outerView instanceof View;
        mManager = newTabPageManager;
        mOuterView = outerView;
        mDelegate = delegate;
    }

    public boolean showItem(@ContextMenuItemId int itemId) {
        switch (itemId) {
            case ID_OPEN_IN_NEW_WINDOW:
                return mManager.isOpenInNewWindowEnabled();
            case ID_OPEN_IN_NEW_TAB:
                return true;
            case ID_OPEN_IN_INCOGNITO_TAB:
                return mManager.isOpenInIncognitoEnabled();
            case ID_SAVE_FOR_OFFLINE:
                return mDelegate.canBeSavedOffline();
            case ID_REMOVE:
                return true;

            default:
                return false;
        }
    }

    public void onCreateContextMenu(ContextMenu menu) {
        if (showItem(ID_OPEN_IN_NEW_WINDOW)) {
            addContextMenuItem(
                    menu, ID_OPEN_IN_NEW_WINDOW, R.string.contextmenu_open_in_other_window);
        }

        if (showItem(ID_OPEN_IN_NEW_TAB)) {
            addContextMenuItem(menu, ID_OPEN_IN_NEW_TAB, R.string.contextmenu_open_in_new_tab);
        }

        if (showItem(ID_OPEN_IN_INCOGNITO_TAB)) {
            addContextMenuItem(
                    menu, ID_OPEN_IN_INCOGNITO_TAB, R.string.contextmenu_open_in_incognito_tab);
        }

        if (showItem(ID_SAVE_FOR_OFFLINE)) {
            addContextMenuItem(menu, ID_SAVE_FOR_OFFLINE, R.string.contextmenu_save_offline);
        }

        if (showItem(ID_REMOVE)) {
            addContextMenuItem(menu, ID_REMOVE, R.string.remove);
        }

        // Disable touch events on the RecyclerView while the context menu is open. This is to
        // prevent the user long pressing to get the context menu then on the same press scrolling
        // or swiping to dismiss an item (eg. https://crbug.com/638854, https://crbug.com/638555,
        // https://crbug.com/636296)
        mOuterView.setTouchEnabled(false);

        mManager.addContextMenuCloseCallback(new Callback<Menu>() {
            @Override
            public void onResult(Menu result) {
                mOuterView.setTouchEnabled(true);
                mManager.removeContextMenuCloseCallback(this);
            }
        });
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        // If the user clicks a snippet then immediately long presses they will create a context
        // menu while the snippet's URL loads in the background. This means that when they press
        // an item on context menu the NTP will not actually be open. We add this check here to
        // prevent taking any action if the user has already left the NTP.
        // https://crbug.com/640468.
        // TODO(peconn): Instead, close the context menu when a snippet is clicked.
        if (!ViewCompat.isAttachedToWindow(mOuterView.asView())) return true;

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

    /**
     * Convenience method to reduce multi-line function call to single line.
     */
    private void addContextMenuItem(ContextMenu menu, @ContextMenuItemId int id, int resourceId) {
        menu.add(Menu.NONE, id, Menu.NONE, resourceId).setOnMenuItemClickListener(this);
    }
}
