// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Environment;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offlinepages.FeatureMode;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private static final String TAG = "OfflinePageUtils";
    /** Snackbar button types */
    public static final int RELOAD_BUTTON = 0;

    private static final int SNACKBAR_DURATION = 6 * 1000; // 6 second

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
     */
    public static boolean isConnected() {
        return NetworkChangeNotifier.isOnline();
    }

    /**
     * Finds out the appropriate resource ID of UI string shown to the user.
     * @param stringResId The resource ID of UI string used when 'bookmarks' name is used  in UI
     *        strings.
     * return The resource ID of UI string shown to the user, depending on the experiment.
     */
    public static int getStringId(int stringResId) {
        if (!OfflinePageBridge.isEnabled()) {
            return stringResId;
        }
        if (OfflinePageBridge.getFeatureMode() != FeatureMode.ENABLED_AS_SAVED_PAGES) {
            return stringResId;
        }
        if (stringResId == R.string.bookmark_action_bar_delete) {
            return R.string.offline_pages_action_bar_delete;
        } else if (stringResId == R.string.bookmark_action_bar_move) {
            return R.string.offline_pages_action_bar_move;
        } else if (stringResId == R.string.bookmark_action_bar_search) {
            return R.string.offline_pages_action_bar_search;
        } else if (stringResId == R.string.edit_bookmark) {
            return R.string.offline_pages_edit_item;
        } else if (stringResId == R.string.bookmark_drawer_all_items) {
            return R.string.offline_pages_all_items;
        } else if (stringResId == R.string.bookmark_title_bar_all_items) {
            return R.string.offline_pages_all_items;
        } else if (stringResId == R.string.bookmarks) {
            return R.string.offline_pages_saved_pages;
        } else if (stringResId == R.string.menu_bookmarks) {
            return R.string.menu_bookmarks_offline_pages;
        } else if (stringResId == R.string.ntp_bookmarks) {
            return R.string.offline_pages_ntp_button_name;
        } else if (stringResId == R.string.accessibility_ntp_toolbar_btn_bookmarks) {
            return R.string.offline_pages_ntp_button_accessibility;
        } else if (stringResId == R.string.bookmarks_folder_empty) {
            return R.string.offline_pages_folder_empty;
        } else if (stringResId == R.string.new_tab_incognito_message) {
            return R.string.offline_pages_new_tab_incognito_message;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_page_saved) {
            return R.string.offline_pages_page_saved;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_page_saved_folder) {
            return R.string.offline_pages_page_saved_folder;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_page_skipped) {
            return R.string.offline_pages_page_skipped;
        } else if (stringResId
                == R.string.offline_pages_as_bookmarks_page_saved_storage_near_full) {
            return R.string.offline_pages_page_saved_storage_near_full;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_page_failed_to_save) {
            return R.string.offline_pages_page_failed_to_save;
        } else if (stringResId
                == R.string.offline_pages_as_bookmarks_page_failed_to_save_storage_near_full) {
            return R.string.offline_pages_page_failed_to_save_storage_near_full;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_storage_space_message) {
            return R.string.offline_pages_storage_space_message;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_viewing_offline_page) {
            return R.string.offline_pages_viewing_offline_page;
        } else if (stringResId == R.string.offline_pages_as_bookmarks_offline_page_size) {
            return R.string.bookmark_offline_page_size;
        } else {
            return stringResId;
        }
    }

    /**
     * Whenever we reload an offline page, if we are online, load the online version of the page
     * instead, on the theory that the user prefers the online version of the page.
     */
    public static void preferOnlineVersion(ChromeActivity activity, Tab tab, String newUrl) {
        // If we are reloading an offline page, but are online, get the online version.
        if (newUrl.equals(tab.getUrl()) && isConnected()) {
            Log.i(TAG, "Refreshing to the online version of an offline page, since we are online");
            tab.loadUrl(new LoadUrlParams(tab.getOfflinePageOriginalUrl(), PageTransition.RELOAD));
        }
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * @param activity The activity owning the tab.
     * @param tab The current tab.
     */
    public static void showOfflineSnackbarIfNecessary(ChromeActivity activity, Tab tab) {
        showOfflineSnackbarIfNecessary(activity, tab, null);
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * This method is used by testing for dependency injecting a snackbar controller.
     * @param activity The activity owning the tab.
     * @param tab The current tab.
     * @param snackbarController Class to show the snackbar.
     */
    public static void showOfflineSnackbarIfNecessary(
            ChromeActivity activity, Tab tab, SnackbarController snackbarController) {
        Log.d(TAG, "showOfflineSnackbarIfNecessary, controller is " + snackbarController);
        if (tab == null || tab.isFrozen()) return;

        if (!OfflinePageBridge.isEnabled()) return;

        // We only show a snackbar if we are seeing an offline page.
        if (!tab.isOfflinePage()) return;

        // Get a snackbar controller if we need one.
        if (snackbarController == null) {
            snackbarController = getSnackbarController(activity, tab);
        }

        final boolean connected = isConnected();

        Log.d(TAG, "showOfflineSnackbarIfNecessary called, tabId " + tab.getId() + ", hidden "
                        + tab.isHidden() + ", connected " + connected + ", controller "
                        + snackbarController);

        // If the tab is no longer hidden, and we have a connection while showing an offline
        // page, offer to reload it now.
        if (!tab.isHidden() && connected) {
            Log.d(TAG, "Offering to reload page, controller " + snackbarController);
            showReloadSnackbar(activity, snackbarController);
            return;
        }

        // Set up the tab observer to watch for the tab being unhidden or connectivity.
        OfflinePageTabObserver.addObserverForTab(
                activity, tab, connected, tab.getUserBookmarkId(), snackbarController);
        return;
    }

    /**
     * Shows the "reload" snackbar for the given tab.
     * @param activity The activity owning the tab.
     * @param snackbarController Class to show the snackbar.
     */
    public static void showReloadSnackbar(final ChromeActivity activity,
            final SnackbarController snackbarController) {
        Log.d(TAG, "showReloadSnackbar called with controller " + snackbarController);
        Context context = activity.getBaseContext();
        final int snackbarTextId = getStringId(R.string.offline_pages_viewing_offline_page);
        Snackbar snackbar = Snackbar.make(context.getString(snackbarTextId), snackbarController,
                                            Snackbar.TYPE_ACTION)
                                    .setAction(context.getString(R.string.reload), RELOAD_BUTTON);
        Log.d(TAG, "made snackbar with controller " + snackbarController);
        snackbar.setDuration(SNACKBAR_DURATION);
        activity.getSnackbarManager().showSnackbar(snackbar);
    }

    /**
     * Gets a snackbar controller that we can use to show our snackbar.
     */
    private static SnackbarController getSnackbarController(
            final ChromeActivity activity, final Tab tab) {
        final int tabId = tab.getId();
        Log.d(TAG, "building snackbar controller");

        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                assert RELOAD_BUTTON == (int) actionData;
                RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                Tab foundTab = activity.getTabModelSelector().getTabById(tabId);
                if (foundTab == null) return;
                foundTab.loadUrl(new LoadUrlParams(
                        foundTab.getOfflinePageOriginalUrl(), PageTransition.RELOAD));
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("OfflinePages.ReloadButtonNotClicked");
            }
        };
    }
}
