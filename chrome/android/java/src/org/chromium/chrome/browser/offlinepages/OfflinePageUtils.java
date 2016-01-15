// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Environment;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkUtils;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private static final String TAG = "OfflinePageUtils";
    /** Snackbar button types */
    private static final int RELOAD_BUTTON = 0;
    private static final int EDIT_BUTTON = 1;

    private static final long STORAGE_ALMOST_FULL_THRESHOLD_BYTES = 10L * (1 << 20); // 10M

    /**
     * Returns the number of free bytes on the storage.
     */
    public static long getFreeSpaceInBytes() {
        return Environment.getExternalStorageDirectory().getUsableSpace();
    }

    /**
     * Returns the number of total bytes on the storage.
     */
    public static long getTotalSpaceInBytes() {
        return Environment.getExternalStorageDirectory().getTotalSpace();
    }

    /**
     * Returns true if the stoarge is almost full which indicates that the user probably needs to
     * free up some space.
     */
    public static boolean isStorageAlmostFull() {
        return Environment.getExternalStorageDirectory().getUsableSpace()
                < STORAGE_ALMOST_FULL_THRESHOLD_BYTES;
    }

    /**
     * Returns true if the network is connected.
     * @param context Context associated with the activity.
     */
    public static boolean isConnected(Context context) {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
        return networkInfo != null && networkInfo.isConnected();
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * @param activity The activity owning the tab.
     * @param tab The current tab.
     */
    public static void showOfflineSnackbarIfNecessary(final ChromeActivity activity, Tab tab) {
        if (tab == null || tab.isFrozen()) {
            return;
        }

        if (!OfflinePageBridge.isEnabled()) {
            return;
        }

        Log.d(TAG, "showOfflineSnackbarIfNecessary called, tab " + tab);

        // We only show a snackbar if we are seeing an offline page.
        if (!tab.isOfflinePage()) return;

        final long bookmarkId = tab.getUserBookmarkId();
        Context context = activity.getBaseContext();
        final boolean connected = isConnected(context);

        if (!tab.isHidden() && connected) {
            Log.d(TAG, "Showing offline page snackbar");
            showReloadSnackbar(activity, tab.getId(), bookmarkId);
            return;
        }

        // Set up the connectivity listener to watch for connectivity.
        tab.addObserver(new OfflinePageTabObserver(activity, tab, connected, bookmarkId));
        return;
    }

    /**
     * Shows the "reload" snackbar for the given tab.
     */
    public static void showReloadSnackbar(
            final ChromeActivity activity, final int tabId, final long bookmarkId) {
        int buttonType = RELOAD_BUTTON;
        int actionTextId = R.string.reload;
        showOfflineSnackbar(activity, tabId, bookmarkId, buttonType, actionTextId);
    }

    public static void showEditSnackbar(
            final ChromeActivity activity, final int tabId, final long bookmarkId) {
        int buttonType = EDIT_BUTTON;
        int actionTextId = R.string.enhanced_bookmark_item_edit;
        showOfflineSnackbar(activity, tabId, bookmarkId, buttonType, actionTextId);
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information.
     * @param activity The activity owning the tab.
     * @param tabId The ID of current tab.
     * @param bookmarkId Bookmark ID related to the opened page.
     */
    private static void showOfflineSnackbar(final ChromeActivity activity, final int tabId,
            final long bookmarkId, final int buttonType, final int actionTextId) {
        Context context = activity.getBaseContext();

        final int snackbarTextId = R.string.offline_pages_viewing_offline_page;

        SnackbarController snackbarController = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                Tab tab = activity.getTabModelSelector().getTabById(tabId);
                if (tab == null) return;

                int buttonType = (int) actionData;
                switch (buttonType) {
                    case RELOAD_BUTTON:
                        RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                        tab.loadUrl(new LoadUrlParams(
                                tab.getOfflinePageOriginalUrl(), PageTransition.RELOAD));
                        break;
                    case EDIT_BUTTON:
                        RecordUserAction.record("OfflinePages.ViewingOffline.EditButtonClicked");
                        EnhancedBookmarkUtils.startEditActivity(
                                activity, new BookmarkId(bookmarkId, BookmarkType.NORMAL), null);
                        break;
                    default:
                        assert false;
                        break;
                }
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                if (actionData == null) return;
                int buttonType = (int) actionData;
                switch (buttonType) {
                    case RELOAD_BUTTON:
                        RecordUserAction.record("OfflinePages.ReloadButtonNotClicked");
                        break;
                    case EDIT_BUTTON:
                        RecordUserAction.record("OfflinePages.ViewingOffline.EditButtonNotClicked");
                        break;
                    default:
                        assert false;
                        break;
                }
            }

            @Override
            public void onDismissForEachType(boolean isTimeout) {}
        };
        Snackbar snackbar = Snackbar.make(context.getString(snackbarTextId), snackbarController)
                                    .setAction(context.getString(actionTextId), buttonType);
        activity.getSnackbarManager().showSnackbar(snackbar);
    }
}
