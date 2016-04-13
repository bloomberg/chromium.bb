// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Environment;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.offlinepages.FeatureMode;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private static final String TAG = "OfflinePageUtils";
    /** Background task tag to differentiate from other task types */
    public static final String TASK_TAG = "OfflinePageUtils";

    private static final int SNACKBAR_DURATION = 6 * 1000; // 6 second

    private static final long STORAGE_ALMOST_FULL_THRESHOLD_BYTES = 10L * (1 << 20); // 10M

    /**
     * Returns the number of free bytes on the storage.
     */
    public static long getFreeSpaceInBytes() {
        return Environment.getDataDirectory().getUsableSpace();
    }

    /**
     * Returns the number of total bytes on the storage.
     */
    public static long getTotalSpaceInBytes() {
        return Environment.getDataDirectory().getTotalSpace();
    }

    /**
     * Returns true if the stoarge is almost full which indicates that the user probably needs to
     * free up some space.
     */
    public static boolean isStorageAlmostFull() {
        return getFreeSpaceInBytes() < STORAGE_ALMOST_FULL_THRESHOLD_BYTES;
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
     * Strips scheme from the original URL of the offline page. This is meant to be used by UI.
     * @param onlineUrl an online URL to from which the scheme is removed
     * @return onlineUrl without the scheme
     */
    public static String stripSchemeFromOnlineUrl(String onlineUrl) {
        onlineUrl = onlineUrl.trim();
        // Offline pages are only saved for https:// and http:// schemes.
        if (onlineUrl.startsWith("https://")) {
            return onlineUrl.substring(8);
        } else if (onlineUrl.startsWith("http://")) {
            return onlineUrl.substring(7);
        } else {
            return onlineUrl;
        }
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * @param activity The activity owning the tab.
     * @param tab The current tab.
     */
    public static void showOfflineSnackbarIfNecessary(ChromeActivity activity, Tab tab) {
        if (!OfflinePageBridge.isEnabled()) return;

        if (OfflinePageTabObserver.getInstance() == null) {
            SnackbarController snackbarController =
                    createReloadSnackbarController(activity.getTabModelSelector());
            OfflinePageTabObserver.init(
                    activity.getBaseContext(), activity.getSnackbarManager(), snackbarController);
        }

        showOfflineSnackbarIfNecessary(tab);
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * This method is used by testing for dependency injecting a snackbar controller.
     * @param context android context
     * @param snackbarManager The snackbar manager to show and dismiss snackbars.
     * @param tab The current tab.
     * @param snackbarController The snackbar controller to control snackbar behavior.
     */
    static void showOfflineSnackbarIfNecessary(Tab tab) {
        // Set up the tab observer to watch for the tab being shown (not hidden) and a valid
        // connection. When both conditions are met a snackbar is shown.
        OfflinePageTabObserver.addObserverForTab(tab);
    }

    /**
     * Shows the "reload" snackbar for the given tab.
     * @param activity The activity owning the tab.
     * @param snackbarController Class to show the snackbar.
     */
    public static void showReloadSnackbar(Context context, SnackbarManager snackbarManager,
            final SnackbarController snackbarController, int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return;

        Log.d(TAG, "showReloadSnackbar called with controller " + snackbarController);
        final int snackbarTextId = getStringId(R.string.offline_pages_viewing_offline_page);
        Snackbar snackbar = Snackbar.make(context.getString(snackbarTextId), snackbarController,
                                            Snackbar.TYPE_ACTION)
                                    .setSingleLine(false)
                                    .setAction(context.getString(R.string.reload), tabId);
        snackbar.setDuration(SNACKBAR_DURATION);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Creates a snackbar controller for a case where "Free up space" button is shown to clean up
     * space taken by the offline pages.
     */
    public static SnackbarController createSnackbarControllerForFreeUpSpaceButton(
            final OfflinePageBridge offlinePageBridge, final SnackbarManager snackbarManager,
            final Activity activity) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                // This method will be called only if the snackbar is dismissed by timeout.
                RecordUserAction.record(
                        "OfflinePages.SaveStatusSnackbar.FreeUpSpaceButtonNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("OfflinePages.SaveStatusSnackbar.FreeUpSpaceButtonClicked");
                Callback<OfflinePageStorageSpacePolicy> callback =
                        getStorageSpacePolicyCallback(offlinePageBridge, snackbarManager, activity);

                OfflinePageStorageSpacePolicy.create(offlinePageBridge, callback);
            }
        };
    }

    private static Callback<OfflinePageStorageSpacePolicy> getStorageSpacePolicyCallback(
            final OfflinePageBridge offlinePageBridge, final SnackbarManager snackbarManager,
            final Activity activity) {
        return new Callback<OfflinePageStorageSpacePolicy>() {
            @Override
            public void onResult(OfflinePageStorageSpacePolicy policy) {
                if (policy.hasPagesToCleanUp()) {
                    OfflinePageFreeUpSpaceCallback callback = new OfflinePageFreeUpSpaceCallback() {
                        @Override
                        public void onFreeUpSpaceDone() {
                            snackbarManager.showSnackbar(
                                    OfflinePageFreeUpSpaceDialog.createStorageClearedSnackbar(
                                            activity));
                        }
                        @Override
                        public void onFreeUpSpaceCancelled() {}
                    };
                    OfflinePageFreeUpSpaceDialog dialog =
                            OfflinePageFreeUpSpaceDialog.newInstance(offlinePageBridge, callback);
                    dialog.show(activity.getFragmentManager(), null);
                } else {
                    OfflinePageOpenStorageSettingsDialog.showDialog(activity);
                }
            }
        };
    }

    /**
     * Gets a snackbar controller that we can use to show our snackbar.
     * @param tabModelSelector used to retrieve a tab by ID
     */
    private static SnackbarController createReloadSnackbarController(
            final TabModelSelector tabModelSelector) {
        Log.d(TAG, "building snackbar controller");

        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                assert actionData != null;
                int tabId = (int) actionData;
                RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                Tab foundTab = tabModelSelector.getTabById(tabId);
                if (foundTab == null) return;

                LoadUrlParams params = new LoadUrlParams(
                        foundTab.getOfflinePageOriginalUrl(), PageTransition.RELOAD);
                foundTab.loadUrl(params);
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("OfflinePages.ReloadButtonNotClicked");
            }
        };
    }

    /**
     * Records UMA data when the Offline Pages Background Load service awakens.
     * @param context android context
     */
    public static void recordWakeupUMA(Context context) {
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        // Note this is a sticky intent, so we aren't really registering a receiver, just getting
        // the sticky intent.  That means that we don't need to unregister the filter later.
        Intent batteryStatus = context.registerReceiver(null, filter);
        if (batteryStatus == null) return;

        // Report charging state.
        RecordHistogram.recordBooleanHistogram(
                "OfflinePages.Wakeup.ConnectedToPower", isPowerConnected(batteryStatus));

        // Report battery percentage.
        RecordHistogram.recordPercentageHistogram(
                "OfflinePages.Wakeup.BatteryPercentage", batteryPercentage(batteryStatus));

        // Report the default network found (or none, if we aren't connected).
        int connectionType = NetworkChangeNotifier.getInstance().getCurrentConnectionType();
        Log.d(TAG, "Found single network of type " + connectionType);
        RecordHistogram.recordEnumeratedHistogram("OfflinePages.Wakeup.NetworkAvailable",
                connectionType, ConnectionType.CONNECTION_LAST + 1);
    }

    private static boolean isPowerConnected(Intent batteryStatus) {
        int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        boolean isConnected = (status == BatteryManager.BATTERY_STATUS_CHARGING
                || status == BatteryManager.BATTERY_STATUS_FULL);
        Log.d(TAG, "Power connected is " + isConnected);
        return isConnected;
    }

    private static int batteryPercentage(Intent batteryStatus) {
        int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (scale == 0) return 0;

        int percentage = (int) Math.round(100 * level / (float) scale);
        Log.d(TAG, "Battery Percentage is " + percentage);
        return percentage;
    }
}
