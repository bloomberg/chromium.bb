// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.net.Uri;
import android.os.AsyncTask;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.appmenu.AppMenu;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.ui.base.LocalizationUtils;

/**
 * Contains logic for whether the update menu item should be shown, whether the update toolbar badge
 * should be shown, and UMA logging for the update menu item.
 */
public class UpdateMenuItemHelper {
    private static final String TAG = "UpdateMenuItemHelper";

    // VariationsAssociatedData configs
    private static final String FIELD_TRIAL_NAME = "UpdateMenuItem";
    private static final String ENABLED_VALUE = "true";
    private static final String ENABLE_UPDATE_MENU_ITEM = "enable_update_menu_item";
    private static final String ENABLE_UPDATE_BADGE = "enable_update_badge";
    private static final String SHOW_SUMMARY = "show_summary";
    private static final String CUSTOM_SUMMARY = "custom_summary";

    // UMA constants for logging whether the menu item was clicked.
    private static final int ITEM_NOT_CLICKED = 0;
    private static final int ITEM_CLICKED_INTENT_LAUNCHED = 1;
    private static final int ITEM_CLICKED_INTENT_FAILED = 2;
    private static final int ITEM_CLICKED_BOUNDARY = 3;

    // UMA constants for logging whether Chrome was updated after the menu item was clicked.
    private static final int UPDATED = 0;
    private static final int NOT_UPDATED = 1;
    private static final int UPDATED_BOUNDARY = 2;

    private static UpdateMenuItemHelper sInstance;
    private static Object sGetInstanceLock = new Object();

    // Whether OmahaClient has already been checked for an update.
    private boolean mAlreadyCheckedForUpdates;

    // Whether an update is available.
    private boolean mUpdateAvailable;

    // URL to direct the user to when Omaha detects a newer version available.
    private String mUpdateUrl;

    // Whether the menu item was clicked. This is used to log the click-through rate.
    private boolean mMenuItemClicked;

    // The bitmap to use for the menu button when the badge is showing.
    private Bitmap mBadgedMenuButtonBitmap;

    /**
     * @return The {@link UpdateMenuItemHelper} instance.
     */
    public static UpdateMenuItemHelper getInstance() {
        synchronized (UpdateMenuItemHelper.sGetInstanceLock) {
            if (sInstance == null) {
                sInstance = new UpdateMenuItemHelper();
                String testMarketUrl = getStringParamValue(ChromeSwitches.MARKET_URL_FOR_TESTING);
                if (!TextUtils.isEmpty(testMarketUrl)) {
                    sInstance.mUpdateUrl = testMarketUrl;
                }
            }
            return sInstance;
        }
    }

    /**
     * Checks if the {@link OmahaClient} knows about an update.
     * @param activity The current {@link ChromeActivity}.
     */
    public void checkForUpdateOnBackgroundThread(final ChromeActivity activity) {
        if (!getBooleanParam(ENABLE_UPDATE_MENU_ITEM)) return;

        ThreadUtils.assertOnUiThread();

        if (mAlreadyCheckedForUpdates) {
            if (activity.isActivityDestroyed()) return;
            activity.onCheckForUpdate(mUpdateAvailable);
            return;
        }

        mAlreadyCheckedForUpdates = true;

        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                if (OmahaClient.isNewerVersionAvailable(activity)) {
                    mUpdateUrl = OmahaClient.getMarketURL(activity);
                    mUpdateAvailable = true;
                } else {
                    mUpdateAvailable = false;
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                if (activity.isActivityDestroyed()) return;
                activity.onCheckForUpdate(mUpdateAvailable);
                recordUpdateHistogram();
            }
        }.execute();
    }

    /**
     * Logs whether an update was performed if the update menu item was clicked.
     * Should be called from ChromeActivity#onStart().
     */
    public void onStart() {
        if (mAlreadyCheckedForUpdates) {
            recordUpdateHistogram();
        }
    }

    /**
     * @param activity The current {@link ChromeActivity}.
     * @return Whether the update menu item should be shown.
     */
    public boolean shouldShowMenuItem(ChromeActivity activity) {
        if (getBooleanParam(ChromeSwitches.FORCE_SHOW_UPDATE_MENU_ITEM)) {
            return true;
        }

        if (!getBooleanParam(ENABLE_UPDATE_MENU_ITEM)) {
            return false;
        }

        return updateAvailable(activity);
    }

    /**
     * @param context The current {@link Context}.
     * @return The string to use for summary text or the empty string if no summary should be shown.
     */
    public String getMenuItemSummaryText(Context context) {
        if (!getBooleanParam(SHOW_SUMMARY)) {
            return "";
        }

        String customSummary = getStringParamValue(CUSTOM_SUMMARY);
        if (!TextUtils.isEmpty(customSummary)) {
            return customSummary;
        }

        return context.getResources().getString(R.string.menu_update_summary_default);
    }

    /**
     * @param activity The current {@link ChromeActivity}.
     * @return Whether the update badge should be shown in the toolbar.
     */
    public boolean shouldShowToolbarBadge(ChromeActivity activity) {
        if (getBooleanParam(ChromeSwitches.FORCE_SHOW_UPDATE_MENU_BADGE)) {
            return true;
        }

        if (!getBooleanParam(ENABLE_UPDATE_BADGE)) {
            return false;
        }

        return updateAvailable(activity);
    }

    /**
     * Handles a click on the update menu item.
     * @param activity The current {@link ChromeActivity}.
     */
    public void onMenuItemClicked(ChromeActivity activity) {
        if (mUpdateUrl == null) return;

        // Fire an intent to open the URL.
        try {
            Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(mUpdateUrl));
            activity.startActivity(launchIntent);
            recordItemClickedHistogram(ITEM_CLICKED_INTENT_LAUNCHED);
            PrefServiceBridge.getInstance().setClickedUpdateMenuItem(true);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Failed to launch Activity for: " + mUpdateUrl);
            recordItemClickedHistogram(ITEM_CLICKED_INTENT_FAILED);
        }
    }

    /**
     * Returns a {@link Bitmap} to use for the menu button with part of the original image removed
     * to simulate a 1dp transparent border around the update badge.
     *
     * @param context The current {@link Context}.
     * @return The {@link Bitmap} to use for the the menu button when showing the update badge.
     */
    public Bitmap getBadgedMenuButtonBitmap(Context context) {
        if (mBadgedMenuButtonBitmap == null) {
            // Punch a hole in the app menu button to create the illusion of a 1dp transparent
            // border around the update badge.
            // Load btn_menu bitmap and use it to initialize a canvas.
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inMutable = true;
            mBadgedMenuButtonBitmap = BitmapFactory.decodeResource(context.getResources(),
                    R.drawable.btn_menu, opts);
            Canvas canvas = new Canvas();
            canvas.setBitmap(mBadgedMenuButtonBitmap);

            // Calculate the dimensions and offsets for the update badge.
            float menuBadgeBackgroundSize = context.getResources().getDimension(
                    R.dimen.menu_badge_background_size);
            float radius = menuBadgeBackgroundSize / 2.f;
            // The design specification calls for 4.5dp right offset and 9dp top offset.
            float xPos = 4.5f * context.getResources().getDisplayMetrics().density + radius;
            float yPos = 9f * context.getResources().getDisplayMetrics().density + radius;

            if (LocalizationUtils.isLayoutRtl()) {
                // In RTL layouts, the badge should be placed to the left of the menu button icon.
                xPos = mBadgedMenuButtonBitmap.getWidth() - xPos;
            }

            // Draw a transparent circle on top of the bitmap, creating a hole.
            Paint paint = new Paint();
            paint.setFlags(Paint.ANTI_ALIAS_FLAG);
            paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
            canvas.drawCircle(xPos, yPos, radius, paint);
        }

        return mBadgedMenuButtonBitmap;
    }

    /**
     * Should be called before the AppMenu is dismissed if the update menu item was clicked.
     */
    public void setMenuItemClicked() {
        mMenuItemClicked = true;
    }

    /**
     * Called when the {@link AppMenu} is dimissed. Logs a histogram immediately if the update menu
     * item was not clicked. If it was clicked, logging is delayed until #onMenuItemClicked().
     */
    public void onMenuDismissed() {
        if (!mMenuItemClicked) {
            recordItemClickedHistogram(ITEM_NOT_CLICKED);
        }
        mMenuItemClicked = false;
    }

    private boolean updateAvailable(ChromeActivity activity) {
        if (!mAlreadyCheckedForUpdates) {
            checkForUpdateOnBackgroundThread(activity);
            return false;
        }

        return mUpdateAvailable;
    }

    private void recordItemClickedHistogram(int action) {
        RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.MenuItem.ActionTakenOnMenuOpen",
                action, ITEM_CLICKED_BOUNDARY);
    }

    private void recordUpdateHistogram() {
        if (PrefServiceBridge.getInstance().getClickedUpdateMenuItem()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "GoogleUpdate.MenuItem.ActionTakenAfterItemClicked",
                    mUpdateAvailable ? NOT_UPDATED : UPDATED, UPDATED_BOUNDARY);
            PrefServiceBridge.getInstance().setClickedUpdateMenuItem(false);
        }
    }

    /**
     * Gets a boolean VariationsAssociatedData parameter, assuming the <paramName>="true" format.
     * Also checks for a command-line switch with the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return Whether the param is defined with a value "true", if there's a command-line
     *         flag present with any value.
     */
    private static boolean getBooleanParam(String paramName) {
        if (CommandLine.getInstance().hasSwitch(paramName)) {
            return true;
        }
        return TextUtils.equals(ENABLED_VALUE,
                VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName));
    }

    /**
     * Gets a String VariationsAssociatedData parameter. Also checks for a command-line switch with
     * the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return The command-line flag value if present, or the param is value if present.
     */
    private static String getStringParamValue(String paramName) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        return value;
    }
}
