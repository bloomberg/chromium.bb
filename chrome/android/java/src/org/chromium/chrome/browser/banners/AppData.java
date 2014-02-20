// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.PendingIntent;
import android.graphics.drawable.Drawable;

/**
 * Stores information about a particular app.
 */
public class AppData {
    // Installation states.
    static final int INSTALL_STATE_NOT_INSTALLED = 0;
    static final int INSTALL_STATE_INSTALLING = 1;
    static final int INSTALL_STATE_INSTALLED = 2;

    // Immutable data about this app.
    private final String mSiteUrl;
    private final String mPackageName;

    // Data returned by the system when queried about the app.
    private String mTitle;
    private String mImageUrl;
    private float mRating;
    private String mInstallButtonText;
    private PendingIntent mDetailsIntent;
    private PendingIntent mInstallIntent;

    // Data that can be updated asynchronously.
    private Drawable mIcon;
    private int mInstallState;

    /**
     * Creates a new AppData for the given page and package.
     * @param siteUrl     URL for the site requesting the banner.
     * @param packageName Name of the package associated with the app.
     */
    public AppData(String siteUrl, String packageName) {
        mSiteUrl = siteUrl;
        mPackageName = packageName;
        mInstallState = INSTALL_STATE_NOT_INSTALLED;
    }

    /**
     * Returns the title to display for the app.
     */
    String title() {
        return mTitle;
    }

    /**
     * Returns the URL where the app icon can be retrieved from.
     */
    String imageUrl() {
        return mImageUrl;
    }

    /**
     * Returns the Drawable depicting the app's icon.
     * @return The Drawable to use as the app icon.
     */
    Drawable icon() {
        return mIcon;
    }

    /**
     * Returns how well the app was rated, on a scale from 0 to 5.
     * @return The rating of the app.
     */
    float rating() {
        return mRating;
    }

    /**
     * Returns text to display on the install button when the app is not installed on the system.
     * @return The String to display.
     */
    String installButtonText() {
        return mInstallButtonText;
    }

    /**
     * Returns the Intent used to send a user to a details page about the app.
     * The IntentSender stored inside dictates what package needs to be launched.
     * @return Intent that triggers the details page.
     */
    PendingIntent detailsIntent() {
        return mDetailsIntent;
    }

    /**
     * Returns the PendingIntent that triggers the install.
     * The IntentSender stored inside dictates what package needs to be launched.
     * @return PendingIntent used to trigger the install.
     */
    PendingIntent installIntent() {
        return mInstallIntent;
    }

    /**
     * Returns the state of the install.
     * @return INSTALL_STATE_* representing the current phase of the installation.
     */
    int installState() {
        return mInstallState;
    }

    /**
     * Stores all of the data about the given app after it's been retrieved.
     * @param title             App title.
     * @param imageUrl          URL where the icon is located.
     * @param rating            Rating of the app.
     * @param installButtonText Text to display on the install button if it's not installed yet.
     * @param detailsIntent     Intent to fire to launch the details page for the app
     * @param installIntent     Intent to fire to trigger the purchase/install process.
     */
    void setPackageInfo(String title, String imageUrl, float rating, String installButtonText,
            PendingIntent detailsIntent, PendingIntent installIntent) {
        mTitle = title;
        mImageUrl = imageUrl;
        mRating = rating;
        mInstallButtonText = installButtonText;
        mDetailsIntent = detailsIntent;
        mInstallIntent = installIntent;
    }

    /**
     * Sets the icon used to depict the app.
     * @param Drawable App icon in Drawable form.
     */
    void setIcon(Drawable icon) {
        mIcon = icon;
    }
}
