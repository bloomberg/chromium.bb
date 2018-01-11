// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Intent;
import android.graphics.Rect;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebSettings;

import org.chromium.content.browser.selection.SelectionPopupControllerImpl;

/**
 * Helper class for {@link WebActionMode} encapsulating
 * {@link android.view.ActionMode}. Exposes the functionality of the class
 * for embedder to provide with the callback instance that interacts with it.
 */
public abstract class ActionModeCallbackHelper {
    private static final String TAG = "ActionModeHelper";

    /** Google search doesn't support requests slightly larger than this. */
    public static final int MAX_SEARCH_QUERY_LENGTH = 1000;

    public static final int MENU_ITEM_SHARE = WebSettings.MENU_ITEM_SHARE;
    public static final int MENU_ITEM_WEB_SEARCH = WebSettings.MENU_ITEM_WEB_SEARCH;
    public static final int MENU_ITEM_PROCESS_TEXT = WebSettings.MENU_ITEM_PROCESS_TEXT;

    public static final EmptyActionCallback EMPTY_CALLBACK = new EmptyActionCallback();

    /**
     * Trim a given string query to be processed safely.
     *
     * @param query a raw query to sanitize.
     * @param maxLength maximum length to which the query will be truncated.
     */
    public static String sanitizeQuery(String query, int maxLength) {
        return SelectionPopupControllerImpl.sanitizeQuery(query, maxLength);
    }

    /**
     * Tell if the platform supports floating type action mode. Used not to repeatedly
     * attempt the creation if the request fails once at the beginning. Also check
     * platform version since the floating type is supported only on M or later version
     * of Android platform.
     */
    public abstract boolean supportsFloatingActionMode();

    /**
     * Empty {@link ActionMode.Callback} that does nothing. Used for {@link #EMPTY_CALLBACK}.
     */
    private static class EmptyActionCallback implements ActionMode.Callback {
        @Override
        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
            return false;
        }

        @Override
        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            return false;
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            return false;
        }

        @Override
        public void onDestroyActionMode(ActionMode mode) {}
    };

    /**
     * @return {@code true} if action mode is started and in proper working state.
     *     if null, action mode was not started or is in finished, destroyed state.
     */
    public abstract boolean isActionModeValid();

    /**
     * @see ActionMode#finish()
     */
    public abstract void finishActionMode();

    /**
     * @return The selected text (empty if no text is selected).
     */
    public abstract String getSelectedText();

    /**
     * Called when the processed text is replied from an activity that supports
     * Intent.ACTION_PROCESS_TEXT.
     * @param resultCode the code that indicates if the activity successfully processed the text
     * @param data the reply that contains the processed text.
     */
    public abstract void onReceivedProcessTextResult(int resultCode, Intent data);

    /**
     * Set the action mode menu items allowed on the content.
     * @param allowedMenuItems bit field of item-flag mapping.
     */
    public abstract void setAllowedMenuItems(int menItems);

    /**
     * @see {@link ActionMode.Callback#onCreateActionMode(ActionMode, Menu)}
     */
    public abstract void onCreateActionMode(ActionMode mode, Menu menu);

    /**
     * @see {@link ActionMode.Callback#onPrepareActionMode(ActionMode, Menu)}
     */
    public abstract boolean onPrepareActionMode(ActionMode mode, Menu menu);

    /**
     * @see {@link ActionMode.Callback#onActionItemClicked(ActionMode, MenuItem)}
     */
    public abstract boolean onActionItemClicked(ActionMode mode, MenuItem item);

    /**
     * @see {@link ActionMode.Callback#onDestroyActionMode(ActionMode)}
     */
    public abstract void onDestroyActionMode();

    /**
     * @see {@link ActionMode.Callback2#onDestroyActionMode(ActionMode)}
     */
    public abstract void onGetContentRect(ActionMode mode, View view, Rect outRect);
}
