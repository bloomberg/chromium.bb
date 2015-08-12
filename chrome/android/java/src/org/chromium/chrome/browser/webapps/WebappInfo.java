// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;

import java.io.ByteArrayOutputStream;

/**
 * Stores info about a web app.
 */
public class WebappInfo {
    private boolean mIsInitialized;
    private String mId;
    private Bitmap mIcon;
    private Uri mUri;
    private String mName;
    private String mShortName;
    private int mOrientation;
    private int mSource;
    private long mThemeColor;

    public static WebappInfo createEmpty() {
        return new WebappInfo();
    }

    private static String titleFromIntent(Intent intent) {
        // The reference to title has been kept for reasons of backward compatibility. For intents
        // and shortcuts which were created before we utilized the concept of name and shortName,
        // we set the name and shortName to be the title.
        String title = intent.getStringExtra(ShortcutHelper.EXTRA_TITLE);
        return title == null ? "" : title;
    }

    public static String nameFromIntent(Intent intent) {
        String name = intent.getStringExtra(ShortcutHelper.EXTRA_NAME);
        return name == null ? titleFromIntent(intent) : name;
    }

    public static String shortNameFromIntent(Intent intent) {
        String shortName = intent.getStringExtra(ShortcutHelper.EXTRA_SHORT_NAME);
        return shortName == null ? titleFromIntent(intent) : shortName;
    }

    /**
     * Construct a WebappInfo.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(Intent intent) {
        String id = intent.getStringExtra(ShortcutHelper.EXTRA_ID);
        String icon = intent.getStringExtra(ShortcutHelper.EXTRA_ICON);
        String url = intent.getStringExtra(ShortcutHelper.EXTRA_URL);
        int orientation = intent.getIntExtra(
                ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        int source = intent.getIntExtra(
                ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        long themeColor = intent.getLongExtra(ShortcutHelper.EXTRA_THEME_COLOR,
                ShortcutHelper.THEME_COLOR_INVALID_OR_MISSING);

        String name = nameFromIntent(intent);
        String shortName = shortNameFromIntent(intent);

        return create(id, url, icon, name, shortName, orientation, source, themeColor);
    }

    /**
     * Construct a WebappInfo.
     * @param id ID for the webapp.
     * @param url URL for the webapp.
     * @param icon Icon to show for the webapp.
     * @param name Name of the webapp.
     * @param shortName The short name of the webapp.
     * @param orientation Orientation of the webapp.
     * @param source Source where the webapp was added from.
     * @param themeColor The theme color of the webapp.
     */
    public static WebappInfo create(String id, String url, String icon, String name,
            String shortName, int orientation, int source, long themeColor) {
        if (id == null || url == null) {
            Log.e("WebappInfo", "Data passed in was incomplete: " + id + ", " + url);
            return null;
        }

        Bitmap favicon = null;
        if (!TextUtils.isEmpty(icon)) {
            byte[] decoded = Base64.decode(icon, Base64.DEFAULT);
            favicon = BitmapFactory.decodeByteArray(decoded, 0, decoded.length);
        }

        Uri uri = Uri.parse(url);
        return new WebappInfo(id, uri, favicon, name, shortName, orientation, source, themeColor);
    }

    private WebappInfo(String id, Uri uri, Bitmap icon, String name,
            String shortName, int orientation, int source, long themeColor) {
        mIcon = icon;
        mId = id;
        mName = name;
        mShortName = shortName;
        mUri = uri;
        mOrientation = orientation;
        mSource = source;
        mThemeColor = themeColor;
        mIsInitialized = mUri != null;
    }

    private WebappInfo() {
    }

    /**
     * Writes all of the data about the webapp into the given Bundle.
     * @param outState Bundle to write data into.
     */
    void writeToBundle(Bundle outState) {
        if (!mIsInitialized) return;

        outState.putString(ShortcutHelper.EXTRA_ID, mId);
        outState.putString(ShortcutHelper.EXTRA_URL, mUri.toString());
        outState.putParcelable(ShortcutHelper.EXTRA_ICON, mIcon);
        outState.putString(ShortcutHelper.EXTRA_NAME, mName);
        outState.putString(ShortcutHelper.EXTRA_SHORT_NAME, mShortName);
        outState.putInt(ShortcutHelper.EXTRA_ORIENTATION, mOrientation);
        outState.putInt(ShortcutHelper.EXTRA_SOURCE, mSource);
        outState.putLong(ShortcutHelper.EXTRA_THEME_COLOR, mThemeColor);
    }

    /**
     * Copies all the fields from the given WebappInfo into this instance.
     * @param newInfo Information about the new webapp.
     */
    void copy(WebappInfo newInfo) {
        mIsInitialized = newInfo.mIsInitialized;
        mIcon = newInfo.mIcon;
        mId = newInfo.mId;
        mUri = newInfo.mUri;
        mName = newInfo.mName;
        mShortName = newInfo.mShortName;
        mOrientation = newInfo.mOrientation;
        mSource = newInfo.mSource;
        mThemeColor = newInfo.mThemeColor;
    }

    public boolean isInitialized() {
        return mIsInitialized;
    }

    public String id() {
        return mId;
    }

    public Uri uri() {
        return mUri;
    }

    public Bitmap icon() {
        return mIcon;
    }

    public String name() {
        return mName;
    }

    public String shortName() {
        return mShortName;
    }

    public int orientation() {
        return mOrientation;
    }

    public int source() {
        return mSource;
    }

    /**
     * Theme color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. mThemeColor is a long because we also need to encode the
     * error state of ShortcutHelper.THEME_COLOR_INVALID_OR_MISSING which is a
     * negative number.
     */
    public long themeColor() {
        return mThemeColor;
    }

    // This is needed for clients that want to send the icon trough an intent.
    public String getEncodedIcon() {
        if (mIcon == null) return "";

        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        mIcon.compress(Bitmap.CompressFormat.PNG, 100, byteArrayOutputStream);
        byte[] byteArray = byteArrayOutputStream.toByteArray();
        return Base64.encodeToString(byteArray, Base64.DEFAULT);
    }
}
