// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ScreenOrientationConstants;
import org.chromium.ui.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.util.UUID;

/**
 * This is a helper class to create shortcuts on the Android home screen.
 */
public class ShortcutHelper {
    public static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    public static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    public static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    // EXTRA_TITLE is present for backward compatibility reasons
    public static final String EXTRA_TITLE = "org.chromium.chrome.browser.webapp_title";
    public static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    public static final String EXTRA_SHORT_NAME = "org.chromium.chrome.browser.webapp_short_name";
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_ORIENTATION = ScreenOrientationConstants.EXTRA_ORIENTATION;
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_THEME_COLOR = "org.chromium.chrome.browser.theme_color";

    // This value is equal to kInvalidOrMissingThemeColor in the C++ content::Manifest struct.
    public static final long THEME_COLOR_INVALID_OR_MISSING = ((long) Integer.MAX_VALUE) + 1;

    /** Observes the data fetching pipeline. */
    public interface ShortcutHelperObserver {
        /** Called when the title of the page is available. */
        void onUserTitleAvailable(String title);

        /** Called when the icon to use in the launcher is available. */
        void onIconAvailable(Bitmap icon);
    }

    /** Broadcasts Intents out Android for adding the shortcut. */
    public static class Delegate {
        /**
         * Broadcasts an intent to all interested BroadcastReceivers.
         * @param context The Context to use.
         * @param intent The intent to broadcast.
         */
        public void sendBroadcast(Context context, Intent intent) {
            context.sendBroadcast(intent);
        }

        /**
         * Returns the name of the fullscreen Activity to use when launching shortcuts.
         */
        public String getFullscreenAction() {
            return WebappLauncherActivity.ACTION_START_WEBAPP;
        }
    }

    private static Delegate sDelegate = new Delegate();

    private final Context mAppContext;
    private final Tab mTab;

    private ShortcutHelperObserver mObserver;
    private boolean mIsInitialized;
    private long mNativeShortcutHelper;

    public ShortcutHelper(Context appContext, Tab tab) {
        mAppContext = appContext;
        mTab = tab;
    }

    /**
     * Gets all the information required to initialize the UI.  The observer will be notified as
     * information required for the shortcut become available.
     * @param observer Observer to notify.
     */
    public void initialize(ShortcutHelperObserver observer) {
        mObserver = observer;
        mNativeShortcutHelper = nativeInitialize(mTab.getWebContents());
    }

    /**
     * Returns whether the object is initialized.
     */
    public boolean isInitialized() {
        return mIsInitialized;
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        nativeDestroy(mNativeShortcutHelper);

        // Make sure the callback isn't run if the tear down happens before
        // onInitialized() is called.
        mObserver = null;
        mNativeShortcutHelper = 0;
    }

    /**
     * Sets the delegate to use.
     */
    @VisibleForTesting
    public static void setDelegateForTests(Delegate delegate) {
        sDelegate = delegate;
    }

    @CalledByNative
    private void onUserTitleAvailable(String title) {
        mObserver.onUserTitleAvailable(title);
    }

    @CalledByNative
    private void onIconAvailable(Bitmap icon) {
        mObserver.onIconAvailable(icon);
        mIsInitialized = true;
    }

    /**
     * Adds a shortcut for the current Tab.
     * @param userRequestedTitle Updated title for the shortcut.
     */
    public void addShortcut(String userRequestedTitle) {
        nativeAddShortcut(mNativeShortcutHelper, userRequestedTitle);
    }

    /**
     * Creates an icon that is acceptable to show on the launcher.
     */
    @CalledByNative
    private static Bitmap finalizeLauncherIcon(
            String url, Bitmap icon, int red, int green, int blue) {
        return BookmarkUtils.createLauncherIcon(
                ApplicationStatus.getApplicationContext(), icon, url, red, green, blue);
    }

    /**
     * Called when we have to fire an Intent to add a shortcut to the homescreen.
     * If the webpage indicated that it was capable of functioning as a webapp, it is added as a
     * shortcut to a webapp Activity rather than as a general bookmark. User is sent to the
     * homescreen as soon as the shortcut is created.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addShortcut(Context context, String url, String userTitle, String name,
            String shortName, Bitmap icon, boolean isWebappCapable, int orientation, int source,
            long themeColor) {
        Intent shortcutIntent;
        if (isWebappCapable) {
            // Encode the icon as a base64 string (Launcher drops Bitmaps in the Intent).
            String encodedIcon = "";
            if (icon != null) {
                ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                icon.compress(Bitmap.CompressFormat.PNG, 100, byteArrayOutputStream);
                byte[] byteArray = byteArrayOutputStream.toByteArray();
                encodedIcon = Base64.encodeToString(byteArray, Base64.DEFAULT);
            }

            // Add the shortcut as a launcher icon for a full-screen Activity.
            shortcutIntent = new Intent();
            shortcutIntent.setAction(sDelegate.getFullscreenAction());
            shortcutIntent.putExtra(EXTRA_ICON, encodedIcon);
            shortcutIntent.putExtra(EXTRA_ID, UUID.randomUUID().toString());
            shortcutIntent.putExtra(EXTRA_NAME, name);
            shortcutIntent.putExtra(EXTRA_SHORT_NAME, shortName);
            shortcutIntent.putExtra(EXTRA_URL, url);
            shortcutIntent.putExtra(EXTRA_ORIENTATION, orientation);
            shortcutIntent.putExtra(EXTRA_MAC, getEncodedMac(context, url));
            shortcutIntent.putExtra(EXTRA_THEME_COLOR, themeColor);
        } else {
            // Add the shortcut as a launcher icon to open in the browser Activity.
            shortcutIntent = BookmarkUtils.createShortcutIntent(url);
        }

        // Always attach a source (one of add to homescreen menu item, app banner, or unknown) to
        // the intent. This allows us to distinguish where a shortcut was added from in metrics.
        shortcutIntent.putExtra(EXTRA_SOURCE, source);
        shortcutIntent.setPackage(context.getPackageName());
        sDelegate.sendBroadcast(
                context, BookmarkUtils.createAddToHomeIntent(shortcutIntent, userTitle, icon, url));

        // Alert the user about adding the shortcut.
        final String shortUrl = UrlUtilities.getDomainAndRegistry(url, true);
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                Context applicationContext = ApplicationStatus.getApplicationContext();
                String toastText =
                        applicationContext.getString(R.string.added_to_homescreen, shortUrl);
                Toast toast = Toast.makeText(applicationContext, toastText, Toast.LENGTH_SHORT);
                toast.show();
            }
        });
    }

    /**
     * @return String that can be used to verify that a WebappActivity is being started by Chrome.
     */
    public static String getEncodedMac(Context context, String url) {
        // The only reason we convert to a String here is because Android inexplicably eats a
        // byte[] when adding the shortcut -- the Bundle received by the launched Activity even
        // lacks the key for the extra.
        byte[] mac = WebappAuthenticator.getMacForUrl(context, url);
        return Base64.encodeToString(mac, Base64.DEFAULT);
    }

    private native long nativeInitialize(WebContents webContents);
    private native void nativeAddShortcut(long nativeShortcutHelper, String userRequestedTitle);
    private native void nativeDestroy(long nativeShortcutHelper);
}
