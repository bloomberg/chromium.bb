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
    private static final String FILTER_CAMERA = "camera";
    private static final String FILTER_COOKIES = "cookies";
    private static final String FILTER_JAVASCRIPT = "javascript";
    private static final String FILTER_DEVICE_LOCATION = "device_location";
    private static final String FILTER_FULLSCREEN = "fullscreen";
    private static final String FILTER_MICROPHONE = "microphone";
    private static final String FILTER_POPUPS = "popups";
    private static final String FILTER_PROTECTED_MEDIA = "protected_content";
    public static final String FILTER_PUSH_NOTIFICATIONS = "push_notifications";
    private static final String FILTER_USE_STORAGE = "use_storage";

    public WebsiteSettingsCategoryFilter() {
    }

    /**
     * Converts a category filter key (see above) to content settings enum.
     */
    public int toContentSettingsType(String key) {
        if (showCameraSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
        } else if (showCookiesSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES;
        } else if (showFullscreenSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_FULLSCREEN;
        } else if (showGeolocationSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION;
        } else if (showJavaScriptSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT;
        } else if (showMicrophoneSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;
        } else if (showPopupSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS;
        } else if (showProtectedMediaSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER;
        } else if (showPushNotificationsSites(key)) {
            return ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
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
     * @return Whether the category passed is the camera category.
     */
    public boolean showCameraSites(String filterValue) {
        return filterValue.equals(FILTER_CAMERA);
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
     * @return Whether the category passed is the javascript category.
     */
    public boolean showJavaScriptSites(String filterValue) {
        return filterValue.equals(FILTER_JAVASCRIPT);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the microphone category.
     */
    public boolean showMicrophoneSites(String filterValue) {
        return filterValue.equals(FILTER_MICROPHONE);
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
     * @return Whether the category passed is the protected media category.
     */
    public boolean showProtectedMediaSites(String filterValue) {
        return filterValue.equals(FILTER_PROTECTED_MEDIA);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the push notification category.
     */
    public boolean showPushNotificationsSites(String filterValue) {
        return filterValue.equals(FILTER_PUSH_NOTIFICATIONS);
    }

    /**
     * @param filterValue A category value.
     * @return Whether the category passed is the storage category.
     */
    public boolean showStorageSites(String filterValue) {
        return filterValue.equals(FILTER_USE_STORAGE);
    }
}
