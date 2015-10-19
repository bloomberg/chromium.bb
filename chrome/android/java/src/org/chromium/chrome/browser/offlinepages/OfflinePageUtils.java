// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.Manifest.permission;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Environment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeBrowserProviderClient;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private enum SnackbarButtonType {
        NONE,
        RELOAD,
        SAVE,
    };

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

        boolean save;
        if (tab.isOfflinePage()) {
            // If an offline page is being visited, prompt that an offline copy is being shown.
            save = false;
        } else if (tab.getUserBookmarkId() != ChromeBrowserProviderClient.INVALID_BOOKMARK_ID
                && !tab.hasOfflineCopy()) {
            // If a bookmarked page without offline copy is being visited, offer to save it.
            save = true;
        } else {
            // Otherwise, no need to show the snackbar.
            return;
        }

        if (tab.isHidden()) {
            // Wait until the tab becomes visible.
            final boolean finalSave = save;
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onShown(Tab visibleTab) {
                    showOfflineSnackbar(activity, visibleTab.getId(), finalSave);
                }
            });
        } else {
            showOfflineSnackbar(activity, tab.getId(), save);
        }
    }

    /**
     * Returns whether file access is allowed.
     *
     * @param view The ContentViewCore to access the file system.
     * @return true if allowed, or false otherwise.
     */
    public static boolean hasFileAccessPermission(ContentViewCore view) {
        return view.getWindowAndroid().hasPermission(permission.WRITE_EXTERNAL_STORAGE);
    }

    /**
     * Called to prompt user with the file access permission.
     *
     * @param view The ContentViewCore to access the file system.
     * @param callback Callback for the permission request.
     */
    public static void requestFileAccessPermission(
            ContentViewCore view, PermissionCallback callback) {
        view.getWindowAndroid().requestPermissions(
                new String[] {android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, callback);
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information.
     * @param activity The activity owning the tab.
     * @param tabId The ID of current tab.
     * @param save Whether to offer saving the page.
     */
    private static void showOfflineSnackbar(
            final ChromeActivity activity, final int tabId, boolean save) {
        Context context = activity.getBaseContext();

        int snackbarTextId = -1;
        int actionTextId = -1;
        SnackbarButtonType buttonType = SnackbarButtonType.NONE;
        if (save) {
            buttonType = SnackbarButtonType.SAVE;
            snackbarTextId = R.string.offline_pages_save_page_offline;
            actionTextId = R.string.save;
        } else {
            snackbarTextId = R.string.offline_pages_viewing_offline_page;

            // Offer to reload the original page if there is network connection.
            if (isConnected(context)) {
                buttonType = SnackbarButtonType.RELOAD;
                actionTextId = R.string.reload;
            }
        }

        SnackbarController snackbarController = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                Tab tab = activity.getTabModelSelector().getTabById(tabId);
                if (tab == null) {
                    return;
                }
                SnackbarButtonType buttonType = (SnackbarButtonType) actionData;
                switch (buttonType) {
                    case RELOAD:
                        RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                        tab.loadUrl(new LoadUrlParams(
                                tab.getOfflinePageOriginalUrl(), PageTransition.RELOAD));
                        break;
                    case SAVE:
                        RecordUserAction.record("OfflinePages.SaveButtonClicked");
                        activity.saveBookmarkOffline(tab);
                        break;
                    default:
                        assert false;
                        break;
                }
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                if (actionData == null) return;
                SnackbarButtonType buttonType = (SnackbarButtonType) actionData;
                switch (buttonType) {
                    case NONE:
                        // No recording is needed.
                        break;
                    case RELOAD:
                        RecordUserAction.record("OfflinePages.ReloadButtonNotClicked");
                        break;
                    case SAVE:
                        RecordUserAction.record("OfflinePages.SaveButtonNotClicked");
                        break;
                    default:
                        assert false;
                        break;
                }
            }

            @Override
            public void onDismissForEachType(boolean isTimeout) {}
        };
        Snackbar snackbar = Snackbar.make(context.getString(snackbarTextId), snackbarController);
        if (actionTextId != -1) {
            snackbar.setAction(context.getString(actionTextId), buttonType);
        }
        activity.getSnackbarManager().showSnackbar(snackbar);
    }
}
