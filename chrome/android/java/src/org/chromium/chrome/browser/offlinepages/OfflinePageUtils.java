// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Environment;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offlinepages.FeatureMode;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeUnit;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private static final String TAG = "OfflinePageUtils";
    /** Snackbar button types */
    public static final int RELOAD_BUTTON = 0;
    public static final int EDIT_BUTTON = 1;

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
     * @param context Context associated with the activity.
     */
    // TODO(petewil): Remove context, we no longer need it now that we use NCN.
    public static boolean isConnected(Context context) {
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
        if (stringResId == R.string.enhanced_bookmark_action_bar_delete) {
            return R.string.offline_pages_action_bar_delete;
        } else if (stringResId == R.string.enhanced_bookmark_action_bar_move) {
            return R.string.offline_pages_action_bar_move;
        } else if (stringResId == R.string.enhanced_bookmark_action_bar_search) {
            return R.string.offline_pages_action_bar_search;
        } else if (stringResId == R.string.edit_bookmark) {
            return R.string.offline_pages_edit_item;
        } else if (stringResId == R.string.enhanced_bookmark_drawer_all_items) {
            return R.string.offline_pages_all_items;
        } else if (stringResId == R.string.enhanced_bookmark_title_bar_all_items) {
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
     * Retrieves the url to launch a bookmark or saved page. If latter, also marks it as
     * accessed and reports the UMAs.
     *
     * @param context Context for checking connection.
     * @param bridge Offline page bridge.
     * @param page Offline page to get the launch url for.
     * @param onlineUrl Online URL to launch if offline is not available.
     * @return The launch URL.
     */
    // TODO(fgorski): Add tests once petewil lands OfflinePageUtilsTest.
    public static String getLaunchUrlAndMarkAccessed(
            Context context, OfflinePageBridge bridge, OfflinePageItem page, String onlineUrl) {
        if (page == null) return onlineUrl;

        boolean isConnected = OfflinePageUtils.isConnected(context);
        RecordHistogram.recordBooleanHistogram("OfflinePages.OnlineOnOpen", isConnected);

        // When there is a network connection, we visit original URL online.
        if (isConnected) return onlineUrl;

        // TODO(fgorski): This code should be moved to markPageAccessed on the native side.
        // The last access time was set to same as creation time when the page was created.
        int maxMinutes = (int) TimeUnit.DAYS.toMinutes(90);
        int minutesSinceLastOpened =
                (int) ((System.currentTimeMillis() - page.getLastAccessTimeMs()) / (1000 * 60));
        if (page.getCreationTimeMs() == page.getLastAccessTimeMs()) {
            RecordHistogram.recordCustomCountHistogram("OfflinePages.FirstOpenSinceCreated",
                    minutesSinceLastOpened, 1, maxMinutes, 50);
        } else {
            RecordHistogram.recordCustomCountHistogram("OfflinePages.OpenSinceLastOpen",
                    minutesSinceLastOpened, 1, maxMinutes, 50);
        }

        // Mark that the offline page has been accessed, that will cause last access time and access
        // count being updated.
        bridge.markPageAccessed(page.getBookmarkId());

        // Returns the offline URL for offline access.
        return page.getOfflineUrl();
    }

    /**
     * Retrieves the url to launch a bookmark or saved page. If latter, also marks it as accessed
     * and reports the UMAs.
     *
     * @parma context Context for checking connection.
     * @param bridge Offline page bridge.
     * @param onlineUrl Online url of a bookmark.
     * @return The launch URL.
     */
    // TODO(fgorski): Add tests once petewil lands OfflinePageUtilsTest.
    public static String getLaunchUrlFromOnlineUrl(
            Context context, OfflinePageBridge bridge, String onlineUrl) {
        if (!OfflinePageBridge.isEnabled() || bridge == null) return onlineUrl;
        return getLaunchUrlAndMarkAccessed(
                context, bridge, bridge.getPageByOnlineURL(onlineUrl), onlineUrl);
    }

    /**
     * Whenever we reload an offline page, if we are online, load the online version of the page
     * instead, on the theory that the user prefers the online version of the page.
     */
    public static void preferOnlineVersion(ChromeActivity activity, Tab tab, String newUrl) {
        // If we are reloading an offline page, but are online, get the online version.
        if (newUrl.equals(tab.getUrl()) && isConnected(activity.getBaseContext())) {
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

        Context context = activity.getBaseContext();
        final boolean connected = isConnected(context);

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
     */
    public static void showReloadSnackbar(final ChromeActivity activity,
            final SnackbarController snackbarController) {
        Log.d(TAG, "showReloadSnackbar called with controller " + snackbarController);
        int buttonType = RELOAD_BUTTON;
        int actionTextId = R.string.reload;
        showOfflineSnackbar(activity, snackbarController, buttonType, actionTextId);
    }

    public static void showEditSnackbar(final ChromeActivity activity,
            final SnackbarController snackbarController) {
        int buttonType = EDIT_BUTTON;
        int actionTextId = R.string.enhanced_bookmark_item_edit;
        showOfflineSnackbar(activity, snackbarController, buttonType, actionTextId);
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information.
     * @param activity The activity owning the tab.
     * @param snackbarController Class to show the snackbar.
     * @param buttonType Which snackbar button to show.
     * @param actionTextId Resource ID of the text to put on the snackbar button.
     */
    public static void showOfflineSnackbar(final ChromeActivity activity,
            final SnackbarController snackbarController, int buttonType, final int actionTextId) {
        Context context = activity.getBaseContext();

        final int snackbarTextId = getStringId(R.string.offline_pages_viewing_offline_page);

        Snackbar snackbar = Snackbar
                .make(context.getString(snackbarTextId), snackbarController, Snackbar.TYPE_ACTION)
                .setAction(context.getString(actionTextId), buttonType);
        Log.d(TAG, "made snackbar with controller " + snackbarController);
        snackbar.setDuration(SNACKBAR_DURATION);
        activity.getSnackbarManager().showSnackbar(snackbar);
    }

    /**
     * Gets a snackbar controller that we can use to show our snackbar.
     */
    private static SnackbarController getSnackbarController(
            final ChromeActivity activity, final Tab tab) {
        final long bookmarkId = tab.getUserBookmarkId();
        final int tabId = tab.getId();
        Log.d(TAG, "building snackbar controller");

        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                Tab foundTab = activity.getTabModelSelector().getTabById(tabId);
                if (foundTab == null) return;
                int buttonType = (int) actionData;
                switch (buttonType) {
                    case RELOAD_BUTTON:
                        RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                        foundTab.loadUrl(new LoadUrlParams(
                                foundTab.getOfflinePageOriginalUrl(), PageTransition.RELOAD));
                        break;
                    case EDIT_BUTTON:
                        RecordUserAction.record("OfflinePages.ViewingOffline.EditButtonClicked");
                        BookmarkUtils.startEditActivity(
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
        };
    }
}
