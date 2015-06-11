// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import com.google.android.apps.chrome.R;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * A model class that parses intent from third-party apps and provides results to
 * {@link CustomTabActivity}.
 */
public class CustomTabIntentDataProvider {
    private static final String TAG = "CustomTabIntentDataProvider";

    /**
     * The category name that the service intent has to have for custom tabs. This
     * category can also be used to generically resolve to the service component for custom tabs.
     */
    public static final String CATEGORY_CUSTOM_TABS = "android.intent.category.CUSTOM_TABS";

    /**
     * Extra used to match the session. This has to be included in the intent to open in
     * a custom tab. Can be -1 if there is no session id taken. Its value is a long returned
     * by {@link ICustomTabsConnectionService#newSession}.
     */
    public static final String EXTRA_CUSTOM_TABS_SESSION_ID =
            "android.support.CUSTOM_TABS:session_id";

    /**
     * Extra used to keep the caller alive. Its value is an Intent.
     */
    public static final String EXTRA_CUSTOM_TABS_KEEP_ALIVE =
            "android.support.CUSTOM_TABS:keep_alive";

    /**
     * Extra that changes the background color for the omnibox. colorRes is an int that specifies a
     * color
     */
    public static final String EXTRA_CUSTOM_TABS_TOOLBAR_COLOR =
            "android.support.CUSTOM_TABS:toolbar_color";

    /**
     * Bundle used for the action button parameters.
     */
    public static final String EXTRA_CUSTOM_TABS_ACTION_BUTTON_BUNDLE =
            "android.support.CUSTOM_TABS:action_button_bundle";

    /**
     * Key that specifies the Bitmap to be used as the image source for the action button.
     */
    public static final String KEY_CUSTOM_TABS_ICON = "android.support.CUSTOM_TABS:icon";

    /**
     * Key that specifies the PendingIntent to launch when the action button or menu item was
     * clicked. Chrome will be calling {@link PendingIntent#send()} on clicks after adding the url
     * as data. The client app can call {@link Intent#getDataString()} to get the url.
     */
    public static final String KEY_CUSTOM_TABS_PENDING_INTENT =
            "android.support.CUSTOM_TABS:pending_intent";

    /**
     * Use an {@code ArrayList<Bundle>} for specifying menu related params. There should be a
     * separate Bundle for each custom menu item.
     */
    public static final String EXTRA_CUSTOM_TABS_MENU_ITEMS =
            "android.support.CUSTOM_TABS:menu_items";

    /**
     * Key for the title of a menu item.
     */
    public static final String KEY_CUSTOM_TABS_MENU_TITLE =
            "android.support.CUSTOM_TABS:menu_title";

    /**
     * Bundle constructed out of ActivityOptions that Chrome will be running when it finishes
     * {@link CustomTabActivity}. A similar ActivityOptions for starting Chrome should be
     * constructed and given to the startActivity() call that launches Chrome.
     */
    public static final String EXTRA_CUSTOM_TABS_EXIT_ANIMATION_BUNDLE =
            "android.support.CUSTOM_TABS:exit_animation_bundle";

    /**
     * An invalid session ID, used when none is provided in the intent.
     */
    public static final long INVALID_SESSION_ID = -1;

    private static final String BUNDLE_PACKAGE_NAME = "android:packageName";
    private static final String BUNDLE_ENTER_ANIMATION_RESOURCE = "android:animEnterRes";
    private static final String BUNDLE_EXIT_ANIMATION_RESOURCE = "android:animExitRes";
    private final long mSessionId;
    private final Intent mKeepAliveServiceIntent;
    private int mToolbarColor;
    private Bitmap mIcon;
    private PendingIntent mActionButtonPendingIntent;
    private List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();
    private Bundle mAnimationBundle;
    // OnFinished listener for PendingIntents. Used for testing only.
    private PendingIntent.OnFinished mOnFinished;

    /**
     * Constructs a {@link CustomTabIntentDataProvider}.
     */
    public CustomTabIntentDataProvider(Intent intent, Context context) {
        if (intent == null) assert false;

        mSessionId = IntentUtils.safeGetLongExtra(
                intent, EXTRA_CUSTOM_TABS_SESSION_ID, INVALID_SESSION_ID);
        retrieveToolbarColor(intent, context);

        mKeepAliveServiceIntent = IntentUtils.safeGetParcelableExtra(
                intent, EXTRA_CUSTOM_TABS_KEEP_ALIVE);

        Bundle actionButtonBundle = IntentUtils.safeGetBundleExtra(
                intent, EXTRA_CUSTOM_TABS_ACTION_BUTTON_BUNDLE);
        if (actionButtonBundle != null) {
            mIcon = IntentUtils.safeGetParcelable(actionButtonBundle, KEY_CUSTOM_TABS_ICON);
            if (!checkBitmapSizeWithinBounds(context, mIcon)) {
                mIcon.recycle();
                mIcon = null;
            } else {
                mActionButtonPendingIntent = IntentUtils.safeGetParcelable(
                        actionButtonBundle, KEY_CUSTOM_TABS_PENDING_INTENT);
            }
        }

        List<Bundle> menuItems = IntentUtils.getParcelableArrayListExtra(
                intent, EXTRA_CUSTOM_TABS_MENU_ITEMS);
        if (menuItems != null) {
            for (Bundle bundle : menuItems) {
                String title = IntentUtils.safeGetString(bundle, KEY_CUSTOM_TABS_MENU_TITLE);
                PendingIntent pendingIntent = IntentUtils.safeGetParcelable(
                        bundle, KEY_CUSTOM_TABS_PENDING_INTENT);
                if (TextUtils.isEmpty(title) || pendingIntent == null) continue;
                mMenuEntries.add(new Pair<String, PendingIntent>(title, pendingIntent));
            }
        }

        mAnimationBundle = IntentUtils.safeGetBundleExtra(
                intent, EXTRA_CUSTOM_TABS_EXIT_ANIMATION_BUNDLE);
    }

    /**
     * Processes the color passed from the client app and updates {@link #mToolbarColor}.
     */
    private void retrieveToolbarColor(Intent intent, Context context) {
        int color = IntentUtils.safeGetIntExtra(intent, EXTRA_CUSTOM_TABS_TOOLBAR_COLOR,
                context.getResources().getColor(R.color.default_primary_color));
        int defaultColor = context.getResources().getColor(R.color.default_primary_color);

        if (color == Color.TRANSPARENT) color = defaultColor;

        // Ignore any transparency value.
        color |= 0xFF000000;

        mToolbarColor = color;
    }

    /**
     * @return The session ID specified in the intent. Will be
     *         INVALID_SESSION_ID if it is not set in the intent.
     */
    public long getSessionId() {
        return mSessionId;
    }

    /**
     * @return The keep alive service intent specified in the intent, or null.
     */
    public Intent getKeepAliveServiceIntent() {
        return mKeepAliveServiceIntent;
    }

    /**
     * @return The toolbar color specified in the intent. Will return the color of
     *         default_primary_color, if not set in the intent.
     */
    public int getToolbarColor() {
        return mToolbarColor;
    }

    /**
     * @return Whether the client app has provided sufficient info for the toolbar to show the
     *         action button.
     */
    public boolean shouldShowActionButton() {
        return mIcon != null && mActionButtonPendingIntent != null;
    }

    /**
     * @return The icon used for the action button. Will be null if not set in the intent.
     */
    public Bitmap getActionButtonIcon() {
        return mIcon;
    }

    /**
     * @return The {@link PendingIntent} that will be sent when the user clicks the action button.
     *         For testing only.
     */
    @VisibleForTesting
    public PendingIntent getActionButtonPendingIntentForTest() {
        return mActionButtonPendingIntent;
    }

    /**
     * @return Titles of menu items that were passed from client app via intent.
     */
    public List<String> getMenuTitles() {
        ArrayList<String> list = new ArrayList<>();
        for (Pair<String, PendingIntent> pair : mMenuEntries) {
            list.add(pair.first);
        }
        return list;
    }

    /**
     * Triggers the client-defined action when the user clicks a custom menu item.
     * @param menuIndex The index that the menu item is shown in the result of
     *                  {@link #getMenuTitles()}
     */
    public void clickMenuItemWithUrl(Context context, int menuIndex, String url) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        try {
            PendingIntent pendingIntent = mMenuEntries.get(menuIndex).second;
            pendingIntent.send(context, 0, addedIntent, mOnFinished, null);
        } catch (CanceledException e) {
            Log.e(TAG, "Custom tab in Chrome failed to send pending intent.");
        }
    }

    /**
     * @return Whether chrome should animate when it finishes. We show animations only if the client
     *         app has supplied the correct animation resources via intent extra.
     */
    public boolean shouldAnimateOnFinish() {
        return mAnimationBundle != null && getClientPackageName() != null;
    }

    /**
     * @return The package name of the client app. This is used for a workaround in order to
     *         retrieve the client's animation resources.
     */
    public String getClientPackageName() {
        if (mAnimationBundle == null) return null;
        return mAnimationBundle.getString(BUNDLE_PACKAGE_NAME);
    }

    /**
     * @return The resource id for enter animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationEnterRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_ENTER_ANIMATION_RESOURCE)
                : 0;
    }

    /**
     * @return The resource id for exit animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationExitRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_EXIT_ANIMATION_RESOURCE)
                : 0;
    }

    /**
     * Send the pending intent for the current action button with the given url as data.
     * @param context The context to use for sending the {@link PendingIntent}.
     * @param url The url to attach as additional data to the {@link PendingIntent}.
     */
    public void sendButtonPendingIntentWithUrl(Context context, String url) {
        assert mActionButtonPendingIntent != null;
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        try {
            mActionButtonPendingIntent.send(context, 0, addedIntent, mOnFinished, null);
        } catch (CanceledException e) {
            Log.e(TAG, "CanceledException while sending pending intent in custom tab");
        }
    }

    private boolean checkBitmapSizeWithinBounds(Context context, Bitmap bitmap) {
        int height = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getHeight() < height) return false;
        int scaledWidth = bitmap.getWidth() / bitmap.getHeight() * height;
        if (scaledWidth > 2 * height) return false;
        return true;
    }

    /**
     * Set the callback object for {@link PendingIntent}s that are sent in this class. For testing
     * purpose only.
     */
    @VisibleForTesting
    void setPendingIntentOnFinishedForTesting(PendingIntent.OnFinished onFinished) {
        mOnFinished = onFinished;
    }
}
