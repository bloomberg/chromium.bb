// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * Website is a class for storing information about a website and its associated permissions.
 */
public class Website implements Serializable {

    static final int INVALID_CAMERA_OR_MICROPHONE_ACCESS = 0;
    static final int CAMERA_ACCESS_ALLOWED = 1;
    static final int MICROPHONE_AND_CAMERA_ACCESS_ALLOWED = 2;
    static final int MICROPHONE_ACCESS_ALLOWED = 3;
    static final int CAMERA_ACCESS_DENIED = 4;
    static final int MICROPHONE_AND_CAMERA_ACCESS_DENIED = 5;
    static final int MICROPHONE_ACCESS_DENIED = 6;

    /**
     * A website permission type (e.g. gelocation or popups) and associated data: the title and
     * icon, and its possible permission states: Allow/Block or Ask/Block, etc.
     */
    public static class PermissionDataEntry {
        /**
         * The resource id of this permission entry's icon.
         */
        public int iconResourceId = 0;

        /**
         * The resource id of this permission entry's title (short version),
         * shown on the Site Settings page and in the global toggle at the
         * top of a Website Settings page.
         */
        public int titleResourceId = 0;

        /**
         * The resource id of this permission entry's title explanation,
         * shown on the Website Details page.
         */
        public int explanationResourceId = 0;

        /**
         * What ContentSetting the global default is set to when enabled.
         * Either Ask/Allow. Not required unless this entry describes a settings
         * that appears on the Site Settings page and has a global toggle.
         */
        public ContentSetting defaultEnabledValue = null;

        /**
         * The ContentSetting for this entry when enabled. Usually Block. Not
         * required unless this entry describes a settings that appears on the
         * Site Settings page and has a global toggle.
         */
        public ContentSetting defaultDisabledValue = null;

        /**
         * The resource ID to use when the enabled state should have a custom
         * summary. When 0 the default string will be used instead.
         */
        public int customEnabledSummary = 0;

        /**
         * The resource ID to use when the disabled state should have a custom
         * summary. When 0 the default string will be used instead.
         */
        public int customDisabledSummary = 0;

        /**
         * Returns the string resource id for a given ContentSetting to show
         * with a permission category.
         * @param value The ContentSetting to convert to string id.
         */
        public int contentSettingToResourceIdForCategory(ContentSetting value) {
            switch (value) {
                case ALLOW:
                    return R.string.website_settings_category_allowed;
                case BLOCK:
                    return R.string.website_settings_category_blocked;
                case ASK:
                    return R.string.website_settings_category_ask;
            }
            return 0;
        }

        /**
         * Returns the string resource id for a given ContentSetting to show
         * with a particular website.
         * @param value The ContentSetting to convert to string id.
         */
        public int contentSettingToResourceIdForSite(ContentSetting value) {
            switch (value) {
                case ALLOW:
                    return R.string.website_settings_permissions_allow;
                case BLOCK:
                    return R.string.website_settings_permissions_block;
                case ASK:
                    return 0;  // We never show Ask as an option on individual permissions.
            }
            return 0;
        }

        /**
         * Fetches the summary (resource id) to show when the entry is enabled.
         */
        public int getEnabledSummaryResourceId() {
            if (customEnabledSummary != 0) {
                return customEnabledSummary;
            } else {
                return contentSettingToResourceIdForCategory(defaultEnabledValue);
            }
        }

        /**
         * Fetches the summary (resource id) to show when the entry is disabled.
         */
        public int getDisabledSummaryResourceId() {
            if (customDisabledSummary != 0) {
                return customDisabledSummary;
            } else {
                return contentSettingToResourceIdForCategory(defaultDisabledValue);
            }
        }

        /**
         * Returns a PermissionDataEntry for a given ContentSettingsType.
         * @param type The ContentSettingsType to look up.
         */
        public static PermissionDataEntry getPermissionDataEntry(int type) {
            PermissionDataEntry entry = null;
            switch (type) {
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_cookie;
                    entry.titleResourceId = R.string.cookies_title;
                    entry.explanationResourceId = R.string.cookies_title;
                    entry.defaultEnabledValue = ContentSetting.ALLOW;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    entry.customEnabledSummary = R.string.website_settings_category_cookie_allowed;
                    entry.customDisabledSummary = R.string.website_settings_category_block_all;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_popups;
                    entry.titleResourceId = R.string.popup_permission_title;
                    entry.explanationResourceId = R.string.popup_permission_title;
                    entry.defaultEnabledValue = ContentSetting.ALLOW;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    entry.customDisabledSummary =
                            R.string.website_settings_category_block_recommended;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_location;
                    entry.titleResourceId = R.string.website_settings_device_location;
                    entry.explanationResourceId = R.string.geolocation_permission_title;
                    entry.defaultEnabledValue = ContentSetting.ASK;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    entry.customEnabledSummary =
                            R.string.website_settings_category_ask_before_accessing;
                    entry.customDisabledSummary = R.string.website_settings_category_block_all;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_media;
                    entry.titleResourceId = R.string.website_settings_use_camera_or_mic;
                    entry.explanationResourceId = 0;  // Programmatically assigned.
                    entry.defaultEnabledValue = ContentSetting.ASK;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    entry.customEnabledSummary =
                            R.string.website_settings_category_ask_before_accessing_camera_mic;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_javascript;
                    entry.titleResourceId = R.string.javascript_permission_title;
                    entry.explanationResourceId = R.string.javascript_permission_title;
                    entry.defaultEnabledValue = ContentSetting.ALLOW;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
                    entry = new PermissionDataEntry();
                    // Midi does not appear as a category on the Site Settings page and
                    // therefore does not need as detailed values.
                    entry.iconResourceId = R.drawable.permission_midi;
                    entry.explanationResourceId = R.string.midi_sysex_permission_title;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                    entry = new PermissionDataEntry();
                    entry.iconResourceId = R.drawable.permission_push_notification;
                    entry.titleResourceId = R.string.push_notifications_permission_title;
                    entry.explanationResourceId = R.string.push_notifications_permission_title;
                    entry.defaultEnabledValue = ContentSetting.ASK;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    entry.customEnabledSummary =
                            R.string.website_settings_category_ask_before_sending;
                    return entry;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
                    entry = new PermissionDataEntry();
                    // Protected Content appears only as a category on the Site Settings page
                    // but does not have a site-list like the others, so it does not need as
                    // detailed values.
                    entry.iconResourceId = R.drawable.permission_protected_media;
                    entry.titleResourceId = org.chromium.chrome.R.string.protected_content;
                    entry.defaultEnabledValue = ContentSetting.ASK;
                    entry.defaultDisabledValue = ContentSetting.BLOCK;
                    return entry;
                default:
                    return null;
            }
        }
    }

    private final WebsiteAddress mAddress;
    private final String mTitle;
    private String mSummary;
    private CookieInfo mCookieInfo;
    private GeolocationInfo mGeolocationInfo;
    private MidiInfo mMidiInfo;
    private JavaScriptExceptionInfo mJavaScriptExceptionInfo;
    private PopupExceptionInfo mPopupExceptionInfo;
    private ProtectedMediaIdentifierInfo mProtectedMediaIdentifierInfo;
    private PushNotificationInfo mPushNotificationInfo;
    private VoiceAndVideoCaptureInfo mVoiceAndVideoCaptureInfo;
    private LocalStorageInfo mLocalStorageInfo;
    private final List<StorageInfo> mStorageInfo = new ArrayList<StorageInfo>();
    private int mStorageInfoCallbacksLeft;

    public Website(WebsiteAddress address) {
        mAddress = address;
        mTitle = address.getTitle();
    }

    public WebsiteAddress getAddress() {
        return mAddress;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getSummary() {
        return mSummary;
    }

    public int compareByAddressTo(Website to) {
        return this == to ? 0 : mAddress.compareTo(to.mAddress);
    }

    /**
     * A comparison function for sorting by storage (most used first).
     * @return which site uses more storage.
     */
    public int compareByStorageTo(Website to) {
        if (this == to) return 0;
        return getTotalUsage() < to.getTotalUsage() ? 1 : -1;
    }

    /**
     * Sets the CookieInfo object for this site.
     */
    public void setCookieInfo(CookieInfo info) {
        mCookieInfo = info;
        WebsiteAddress embedder = WebsiteAddress.create(info.getEmbedder());
        if (embedder != null) {
            mSummary = embedder.getTitle();
        }
    }

    public CookieInfo getCookieInfo() {
        return mCookieInfo;
    }

    /**
     * Gets the permission that governs cookie preferences.
     */
    public ContentSetting getCookiePermission() {
        return mCookieInfo != null ? mCookieInfo.getContentSetting() : null;
    }

    /**
     * Sets the permission that govers cookie preferences for this site.
     */
    public void setCookiePermission(ContentSetting value) {
        if (mCookieInfo != null) {
            mCookieInfo.setContentSetting(value);
        }
    }

    /**
     * Sets the GeoLocationInfo object for this Website.
     */
    public void setGeolocationInfo(GeolocationInfo info) {
        mGeolocationInfo = info;
        WebsiteAddress embedder = WebsiteAddress.create(info.getEmbedder());
        if (embedder != null) {
            mSummary = embedder.getTitle();
        }
    }

    public GeolocationInfo getGeolocationInfo() {
        return mGeolocationInfo;
    }

    /**
     * Returns what permission governs geolocation access.
     */
    public ContentSetting getGeolocationPermission() {
        return mGeolocationInfo != null ? mGeolocationInfo.getContentSetting() : null;
    }

    /**
     * Configure geolocation access setting for this site.
     */
    public void setGeolocationPermission(ContentSetting value) {
        if (mGeolocationInfo != null) {
            mGeolocationInfo.setContentSetting(value);
        }
    }

    /**
     * Sets the MidiInfo object for this Website.
     */
    public void setMidiInfo(MidiInfo info) {
        mMidiInfo = info;
        WebsiteAddress embedder = WebsiteAddress.create(info.getEmbedder());
        if (embedder != null) {
            mSummary = embedder.getTitle();
        }
    }

    public MidiInfo getMidiInfo() {
        return mMidiInfo;
    }

    /**
     * Returns what permission governs MIDI usage access.
     */
    public ContentSetting getMidiPermission() {
        return mMidiInfo != null ? mMidiInfo.getContentSetting() : null;
    }

    /**
     * Configure Midi usage access setting for this site.
     */
    public void setMidiPermission(ContentSetting value) {
        if (mMidiInfo != null) {
            mMidiInfo.setContentSetting(value);
        }
    }

    /**
     * Returns what permission governs JavaScript access.
     */
    public ContentSetting getJavaScriptPermission() {
        return mJavaScriptExceptionInfo != null
                ? mJavaScriptExceptionInfo.getContentSetting() : null;
    }

    /**
     * Configure JavaScript permission access setting for this site.
     */
    public void setJavaScriptPermission(ContentSetting value) {
        if (mJavaScriptExceptionInfo != null) {
            mJavaScriptExceptionInfo.setContentSetting(value);
        }
    }

    /**
     * Sets the JavaScript exception info for this Website.
     */
    public void setJavaScriptExceptionInfo(JavaScriptExceptionInfo info) {
        mJavaScriptExceptionInfo = info;
    }

    /**
     * Sets the Popup exception info for this Website.
     */
    public void setPopupExceptionInfo(PopupExceptionInfo info) {
        mPopupExceptionInfo = info;
    }

    public PopupExceptionInfo getPopupExceptionInfo() {
        return mPopupExceptionInfo;
    }

    /**
     * Returns what permission governs Popup permission.
     */
    public ContentSetting getPopupPermission() {
        if (mPopupExceptionInfo != null) return mPopupExceptionInfo.getContentSetting();
        return null;
    }

    /**
     * Configure Popup permission access setting for this site.
     */
    public void setPopupPermission(ContentSetting value) {
        if (mPopupExceptionInfo != null) {
            mPopupExceptionInfo.setContentSetting(value);
        }
    }

    /**
     * Sets protected media identifier access permission information class.
     */
    public void setProtectedMediaIdentifierInfo(ProtectedMediaIdentifierInfo info) {
        mProtectedMediaIdentifierInfo = info;
        WebsiteAddress embedder = WebsiteAddress.create(info.getEmbedder());
        if (embedder != null) {
            mSummary = embedder.getTitle();
        }
    }

    public ProtectedMediaIdentifierInfo getProtectedMediaIdentifierInfo() {
        return mProtectedMediaIdentifierInfo;
    }

    /**
     * Returns what permission governs Protected Media Identifier access.
     */
    public ContentSetting getProtectedMediaIdentifierPermission() {
        return mProtectedMediaIdentifierInfo != null
                ? mProtectedMediaIdentifierInfo.getContentSetting() : null;
    }

    /**
     * Configure Protected Media Identifier access setting for this site.
     */
    public void setProtectedMediaIdentifierPermission(ContentSetting value) {
        if (mProtectedMediaIdentifierInfo != null) {
            mProtectedMediaIdentifierInfo.setContentSetting(value);
        }
    }

    /**
     * Sets Push Notification access permission information class.
     */
    public void setPushNotificationInfo(PushNotificationInfo info) {
        mPushNotificationInfo = info;
    }

    public PushNotificationInfo getPushNotificationInfo() {
        return mPushNotificationInfo;
    }

    /**
     * Returns what setting governs push notification access.
     */
    public ContentSetting getPushNotificationPermission() {
        return mPushNotificationInfo != null ? mPushNotificationInfo.getContentSetting() : null;
    }

    /**
     * Configure push notification setting for this site.
     */
    public void setPushNotificationPermission(ContentSetting value) {
        if (mPushNotificationInfo != null) {
            mPushNotificationInfo.setContentSetting(value);
        }
    }

    /**
     * Sets voice and video capture info class.
     */
    public void setVoiceAndVideoCaptureInfo(VoiceAndVideoCaptureInfo info) {
        mVoiceAndVideoCaptureInfo = info;
        WebsiteAddress embedder = WebsiteAddress.create(info.getEmbedder());
        if (embedder != null) {
            mSummary = embedder.getTitle();
        }
    }

    public VoiceAndVideoCaptureInfo getVoiceAndVideoCaptureInfo() {
        return mVoiceAndVideoCaptureInfo;
    }

    /**
     * Returns what setting governs voice capture access.
     */
    public ContentSetting getVoiceCapturePermission() {
        return mVoiceAndVideoCaptureInfo != null
                ? mVoiceAndVideoCaptureInfo.getVoiceCapturePermission() : null;
    }

    /**
     * Returns what setting governs video capture access.
     */
    public ContentSetting getVideoCapturePermission() {
        return mVoiceAndVideoCaptureInfo != null
                ? mVoiceAndVideoCaptureInfo.getVideoCapturePermission() : null;
    }

    /**
     * Configure voice capture setting for this site.
     */
    public void setVoiceCapturePermission(ContentSetting value) {
        if (mVoiceAndVideoCaptureInfo != null) {
            mVoiceAndVideoCaptureInfo.setVoiceCapturePermission(value);
        }
    }

    /**
     * Configure video capture setting for this site.
     */
    public void setVideoCapturePermission(ContentSetting value) {
        if (mVoiceAndVideoCaptureInfo != null) {
            mVoiceAndVideoCaptureInfo.setVideoCapturePermission(value);
        }
    }

    /**
     * Returns the type of media that is being captured (audio/video/both).
     */
    public int getMediaAccessType() {
        ContentSetting voice = getVoiceCapturePermission();
        ContentSetting video = getVideoCapturePermission();
        if (video != null) {
            if (voice == null) {
                if (video == ContentSetting.ALLOW) {
                    return CAMERA_ACCESS_ALLOWED;
                } else {
                    return CAMERA_ACCESS_DENIED;
                }
            } else {
                if (video != voice) {
                    return INVALID_CAMERA_OR_MICROPHONE_ACCESS;
                }
                if (video == ContentSetting.ALLOW && voice == ContentSetting.ALLOW) {
                    return MICROPHONE_AND_CAMERA_ACCESS_ALLOWED;
                } else {
                    return MICROPHONE_AND_CAMERA_ACCESS_DENIED;
                }
            }
        } else {
            if (voice == null) return INVALID_CAMERA_OR_MICROPHONE_ACCESS;
            if (voice == ContentSetting.ALLOW) {
                return MICROPHONE_ACCESS_ALLOWED;
            } else {
                return MICROPHONE_ACCESS_DENIED;
            }
        }
    }

    public void setLocalStorageInfo(LocalStorageInfo info) {
        mLocalStorageInfo = info;
    }

    public LocalStorageInfo getLocalStorageInfo() {
        return mLocalStorageInfo;
    }

    public void addStorageInfo(StorageInfo info) {
        mStorageInfo.add(info);
    }

    public List<StorageInfo> getStorageInfo() {
        return new ArrayList<StorageInfo>(mStorageInfo);
    }

    public void clearAllStoredData(final StoredDataClearedCallback callback) {
        if (mLocalStorageInfo != null) {
            mLocalStorageInfo.clear();
            mLocalStorageInfo = null;
        }
        mStorageInfoCallbacksLeft = mStorageInfo.size();
        if (mStorageInfoCallbacksLeft > 0) {
            for (StorageInfo info : mStorageInfo) {
                info.clear(new WebsitePreferenceBridge.StorageInfoClearedCallback() {
                    @Override
                    public void onStorageInfoCleared() {
                        if (--mStorageInfoCallbacksLeft == 0) callback.onStoredDataCleared();
                    }
                });
            }
            mStorageInfo.clear();
        } else {
            callback.onStoredDataCleared();
        }
    }

    /**
     * An interface to implement to get a callback when storage info has been cleared.
     */
    public interface StoredDataClearedCallback {
        public void onStoredDataCleared();
    }

    public long getTotalUsage() {
        long usage = 0;
        if (mLocalStorageInfo != null) {
            usage += mLocalStorageInfo.getSize();
        }
        for (StorageInfo info : mStorageInfo) {
            usage += info.getSize();
        }
        return usage;
    }
}
