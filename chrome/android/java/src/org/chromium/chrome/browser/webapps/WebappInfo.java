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
    private String mTitle;
    private int mOrientation;

    public static WebappInfo createEmpty() {
        return new WebappInfo();
    }

    /**
     * Construct a WebappInfo.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(Intent intent) {
        String id = intent.getStringExtra(ShortcutHelper.EXTRA_ID);
        String icon = intent.getStringExtra(ShortcutHelper.EXTRA_ICON);
        String title = intent.getStringExtra(ShortcutHelper.EXTRA_TITLE);
        String url = intent.getStringExtra(ShortcutHelper.EXTRA_URL);
        int orientation = intent.getIntExtra(
                ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        return create(id, url, icon, title, orientation);
    }

    /**
     * Construct a WebappInfo.
     * @param id ID for the webapp.
     * @param url URL for the webapp.
     * @param icon Icon to show for the webapp.
     * @param title Title of the webapp.
     * @param orientation Orientation of the webapp.
     */
    public static WebappInfo create(String id, String url, String icon, String title,
            int orientation) {
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
        return new WebappInfo(id, uri, favicon, title, orientation);
    }

    private WebappInfo(String id, Uri uri, Bitmap icon, String title, int orientation) {
        mIcon = icon;
        mId = id;
        mTitle = title;
        mUri = uri;
        mOrientation = orientation;
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
        outState.putString(ShortcutHelper.EXTRA_TITLE, mTitle);
        outState.putInt(ShortcutHelper.EXTRA_ORIENTATION, mOrientation);
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
        mTitle = newInfo.mTitle;
        mOrientation = newInfo.mOrientation;
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

    public String title() {
        return mTitle;
    }

    public int orientation() {
        return mOrientation;
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
