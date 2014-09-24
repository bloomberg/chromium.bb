// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.content_public.common.ScreenOrientationConstants;

import java.io.ByteArrayOutputStream;
import java.util.UUID;

/**
 * This is a helper class to create shortcuts on the Android home screen.
 */
public class ShortcutHelper {
    public static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    public static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    public static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    public static final String EXTRA_TITLE = "org.chromium.chrome.browser.webapp_title";
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_ORIENTATION = ScreenOrientationConstants.EXTRA_ORIENTATION;

    private static String sFullScreenAction;

    /**
     * Sets the class name used when launching the shortcuts.
     * @param fullScreenAction Class name of the fullscreen Activity.
     */
    public static void setFullScreenAction(String fullScreenAction) {
        sFullScreenAction = fullScreenAction;
    }

    /**
     * Callback to be passed to the initialized() method.
     */
    public interface OnInitialized {
        public void onInitialized(String title);
    }

    private final Context mAppContext;
    private final Tab mTab;

    private OnInitialized mCallback;
    private boolean mIsInitialized;
    private long mNativeShortcutHelper;

    public ShortcutHelper(Context appContext, Tab tab) {
        mAppContext = appContext;
        mTab = tab;
    }

    /**
     * Gets all the information required to initialize the UI, the passed
     * callback will be run when those information will be available.
     * @param callback Callback to be run when initialized.
     */
    public void initialize(OnInitialized callback) {
        mCallback = callback;
        mNativeShortcutHelper = nativeInitialize(mTab.getNativePtr());
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
    public void tearDown() {
        nativeTearDown(mNativeShortcutHelper);

        // Make sure the callback isn't run if the tear down happens before
        // onInitialized() is called.
        mCallback = null;
        mNativeShortcutHelper = 0;
    }

    @CalledByNative
    private void onInitialized(String title) {
        mIsInitialized = true;

        if (mCallback != null) {
            mCallback.onInitialized(title);
        }
    }

    /**
     * Adds a shortcut for the current Tab.
     * @param userRequestedTitle Updated title for the shortcut.
     */
    public void addShortcut(String userRequestedTitle) {
        if (TextUtils.isEmpty(sFullScreenAction)) {
            Log.e("ShortcutHelper", "ShortcutHelper is uninitialized.  Aborting.");
            return;
        }
        ActivityManager am = (ActivityManager) mAppContext.getSystemService(
                Context.ACTIVITY_SERVICE);
        nativeAddShortcut(mNativeShortcutHelper, userRequestedTitle, am.getLauncherLargeIconSize());

        // The C++ instance is no longer owned by the Java object.
        mCallback = null;
        mNativeShortcutHelper = 0;
    }

    /**
     * Called when we have to fire an Intent to add a shortcut to the homescreen.
     * If the webpage indicated that it was capable of functioning as a webapp, it is added as a
     * shortcut to a webapp Activity rather than as a general bookmark. User is sent to the
     * homescreen as soon as the shortcut is created.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addShortcut(Context context, String url, String title, Bitmap icon,
            int red, int green, int blue, boolean isWebappCapable, int orientation) {
        assert sFullScreenAction != null;

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
            shortcutIntent.setAction(sFullScreenAction);
            shortcutIntent.putExtra(EXTRA_ICON, encodedIcon);
            shortcutIntent.putExtra(EXTRA_ID, UUID.randomUUID().toString());
            shortcutIntent.putExtra(EXTRA_TITLE, title);
            shortcutIntent.putExtra(EXTRA_URL, url);
            shortcutIntent.putExtra(EXTRA_ORIENTATION, orientation);

            // The only reason we convert to a String here is because Android inexplicably eats a
            // byte[] when adding the shortcut -- the Bundle received by the launched Activity even
            // lacks the key for the extra.
            byte[] mac = WebappAuthenticator.getMacForUrl(context, url);
            String encodedMac = Base64.encodeToString(mac, Base64.DEFAULT);
            shortcutIntent.putExtra(EXTRA_MAC, encodedMac);
        } else {
            // Add the shortcut as a launcher icon to open in the browser Activity.
            shortcutIntent = BookmarkUtils.createShortcutIntent(context, url);
        }

        shortcutIntent.setPackage(context.getPackageName());
        context.sendBroadcast(BookmarkUtils.createAddToHomeIntent(context, shortcutIntent, title,
                icon, red, green, blue));

        // User is sent to the homescreen as soon as the shortcut is created.
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        homeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(homeIntent);
    }

    private native long nativeInitialize(long tabAndroidPtr);
    private native void nativeAddShortcut(long nativeShortcutHelper, String userRequestedTitle,
            int launcherLargeIconSize);
    private native void nativeTearDown(long nativeShortcutHelper);
}
