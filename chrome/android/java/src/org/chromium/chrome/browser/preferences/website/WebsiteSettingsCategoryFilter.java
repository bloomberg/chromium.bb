// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.ContentSettingsType;

/**
 * A helper class for dealing with website settings category filter.
 */
public class WebsiteSettingsCategoryFilter {
    // The actual values for the keys for the category filter.
    private static final String FILTER_ALL_SITES = "all_sites";
    private static final String FILTER_COOKIES = "cookies";
    private static final String FILTER_CAMERA_MIC = "use_camera_or_mic";
    private static final String FILTER_JAVASCRIPT = "javascript";
    private static final String FILTER_DEVICE_LOCATION = "device_location";
    private static final String FILTER_FULLSCREEN = "fullscreen";
    private static final String FILTER_USE_STORAGE = "use_storage";
    private static final String FILTER_POPUPS = "popups";
    public static final String FILTER_PUSH_NOTIFICATIONS = "push_notifications";

    public WebsiteSettingsCategoryFilter() {
    }

    /**
     * Converts a category filter key (see above) to content settings enum.
     */
    public int toContentSettingsType(String key) {
        if (showCookiesSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES;
        } else if (showCameraMicSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM;
        } else if (showPopupSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS;
        } else if (showGeolocationSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION;
        } else if (showPushNotificationsSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
        } else if (showJavaScriptSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT;
        } else if (showFullscreenSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_FULLSCREEN;
        }
        return -1;
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the no-category.
     */
    public boolean showAllSites(String filterValue) {
        return filterValue.isEmpty() || filterValue.equals(FILTER_ALL_SITES);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the cookies category.
     */
    public boolean showCookiesSites(String filterValue) {
        return filterValue.equals(FILTER_COOKIES);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the camera/mic category.
     */
    public boolean showCameraMicSites(String filterValue) {
        return filterValue.equals(FILTER_CAMERA_MIC);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the javascript category.
     */
    public boolean showJavaScriptSites(String filterValue) {
        return filterValue.equals(FILTER_JAVASCRIPT);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the fullscreen category.
     */
    public boolean showFullscreenSites(String filterValue) {
        return filterValue.equals(FILTER_FULLSCREEN);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the geolocation category.
     */
    public boolean showGeolocationSites(String filterValue) {
        return filterValue.equals(FILTER_DEVICE_LOCATION);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the storage category.
     */
    public boolean showStorageSites(String filterValue) {
        return filterValue.equals(FILTER_USE_STORAGE);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the popup category.
     */
    public boolean showPopupSites(String filterValue) {
        return filterValue.equals(FILTER_POPUPS);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the push notification category.
     */
    public boolean showPushNotificationsSites(String filterValue) {
        return filterValue.equals(FILTER_PUSH_NOTIFICATIONS);
    }
}
