// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.widget.ListAdapter;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;

/**
 * Shows a list of HTML5 settings for a single website.
 */
public class SingleWebsitePreferences extends PreferenceFragment
        implements DialogInterface.OnClickListener, OnPreferenceChangeListener,
                OnPreferenceClickListener {
    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";

    // Preference keys, see single_website_preferences.xml
    // Headings:
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_USAGE = "site_usage";
    public static final String PREF_PERMISSIONS = "site_permissions";
    // Actions at the top (if adding new, see hasUsagePreferences below):
    public static final String PREF_CLEAR_DATA = "clear_data";
    // Buttons:
    public static final String PREF_RESET_SITE = "reset_site_button";
    // Website permissions (if adding new, see hasPermissionsPreferences and resetSite below):
    public static final String PREF_COOKIES_PERMISSION = "cookies_permission_list";
    public static final String PREF_LOCATION_ACCESS = "location_access_list";
    public static final String PREF_MIDI_SYSEX_PERMISSION = "midi_sysex_permission_list";
    public static final String PREF_POPUP_PERMISSION = "popup_permission_list";
    public static final String PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION =
            "protected_media_identifier_permission_list";
    public static final String PREF_PUSH_NOTIFICATIONS_PERMISSION =
            "push_notifications_list";
    public static final String PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION =
            "voice_and_video_capture_permission_list";

    // The website this page is displaying details about.
    private Website mSite;
    // A list of possible options for each list preference summary.
    private String[] mListPreferenceSummaries;

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        getActivity().setTitle(R.string.prefs_content_settings);
        mSite = (Website) getArguments().getSerializable(EXTRA_SITE);
        addPreferencesFromResource(R.xml.single_website_preferences);
        mListPreferenceSummaries = getActivity().getResources().getStringArray(
                R.array.website_settings_permission_options);

        ListAdapter preferences = getPreferenceScreen().getRootAdapter();
        for (int i = 0; i < preferences.getCount(); ++i) {
            Preference preference = (Preference) preferences.getItem(i);
            if (PREF_SITE_TITLE.equals(preference.getKey())) {
                preference.setTitle(mSite.getTitle());
            } else if (PREF_CLEAR_DATA.equals(preference.getKey())) {
                long usage = mSite.getTotalUsage();
                if (usage > 0) {
                    Context context = preference.getContext();
                    preference.setTitle(String.format(
                            context.getString(R.string.origin_settings_storage_usage_brief),
                            WebsitePreference.sizeValueToString(context, usage)));
                    ((ClearWebsiteStorage) preference).setConfirmationListener(this);
                } else {
                    getPreferenceScreen().removePreference(preference);
                }
            } else if (PREF_RESET_SITE.equals(preference.getKey())) {
                preference.setOnPreferenceClickListener(this);
            } else if (PREF_COOKIES_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getCookiePermission());
            } else if (PREF_LOCATION_ACCESS.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getGeolocationPermission());
            } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getMidiPermission());
            } else if (PREF_POPUP_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getPopupPermission());
            } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getProtectedMediaIdentifierPermission());
            } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preference.getKey())) {
                if (ContentPreferences.pushNotificationsSupported()) {
                    setUpListPreference(preference, mSite.getPushNotificationPermission());
                } else {
                    getPreferenceScreen().removePreference(preference);
                }
            } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preference.getKey())) {
                configureVoiceAndVideoPreference(preference);
            }
        }

        // Remove categories if no sub-items.
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        if (!hasUsagePreferences()) {
            Preference heading = preferenceScreen.findPreference(PREF_USAGE);
            preferenceScreen.removePreference(heading);
        }
        if (!hasPermissionsPreferences()) {
            Preference heading = preferenceScreen.findPreference(PREF_PERMISSIONS);
            preferenceScreen.removePreference(heading);
        }

        super.onActivityCreated(savedInstanceState);
    }

    private boolean hasUsagePreferences() {
        // New actions under the Usage preference category must be listed here so that the category
        // heading can be removed when no actions are shown.
        return getPreferenceScreen().findPreference(PREF_CLEAR_DATA) != null;
    }

    private boolean hasPermissionsPreferences() {
        // New permissions (from the Permissions preference category) must be listed here so that
        // category headings can be removed when no permissions are shown.
        PreferenceScreen screen = getPreferenceScreen();
        return screen.findPreference(PREF_COOKIES_PERMISSION) != null
                || screen.findPreference(PREF_LOCATION_ACCESS) != null
                || screen.findPreference(PREF_MIDI_SYSEX_PERMISSION) != null
                || screen.findPreference(PREF_POPUP_PERMISSION) != null
                || screen.findPreference(PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION) != null
                || screen.findPreference(PREF_PUSH_NOTIFICATIONS_PERMISSION) != null
                || screen.findPreference(PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION) != null;
    }

    /**
     * Initialize a ListPreference with a certain value.
     * @param preference The ListPreference to initialize.
     * @param value The value to initialize it to.
     */
    private void setUpListPreference(Preference preference, ContentSetting value) {
        if (value == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }

        ListPreference listPreference = (ListPreference) preference;

        Website.PermissionDataEntry entry = getPermissionDataEntry(preference.getKey());
        CharSequence[] keys = new String[2];
        CharSequence[] descriptions = new String[2];
        keys[0] = ContentSetting.ALLOW.toString();
        keys[1] = ContentSetting.BLOCK.toString();
        descriptions[0] = getResources().getString(
                entry.contentSettingToResourceIdForSite(ContentSetting.ALLOW));
        descriptions[1] = getResources().getString(
                entry.contentSettingToResourceIdForSite(ContentSetting.BLOCK));
        listPreference.setEntryValues(keys);
        listPreference.setEntries(descriptions);
        int index = (value == ContentSetting.ALLOW ? 0 : 1);
        listPreference.setValueIndex(index);
        if (entry.explanationResourceId != 0) {
            listPreference.setTitle(entry.explanationResourceId);
        }
        listPreference.setIcon(entry.iconResourceId);

        preference.setSummary(mListPreferenceSummaries[index]);
        listPreference.setOnPreferenceChangeListener(this);
    }

    private Website.PermissionDataEntry getPermissionDataEntry(String preferenceKey) {
        if (PREF_COOKIES_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES);
        } else if (PREF_LOCATION_ACCESS.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION);
        } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
        } else if (PREF_POPUP_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS);
        } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
        } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
        } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM);
        }
        return null;
    }


    /**
     * Based on the type of media allowed or denied for this website, the title and summary
     * of the CheckBoxPreference will change. If this website has no media related permission, then
     * the preference will be removed.
     * @param preference CheckboxPreference whose title and summary will be set.
     */
    private void configureVoiceAndVideoPreference(Preference preference) {
        ContentSetting voice = mSite.getVoiceCapturePermission();
        ContentSetting video = mSite.getVideoCapturePermission();
        if (voice == null && video == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }

        int mediaAccessType = mSite.getMediaAccessType();
        switch (mediaAccessType) {
            case Website.CAMERA_ACCESS_ALLOWED:
            case Website.CAMERA_ACCESS_DENIED:
                preference.setTitle(R.string.video_permission_title);
                break;
            case Website.MICROPHONE_ACCESS_ALLOWED:
            case Website.MICROPHONE_ACCESS_DENIED:
                preference.setTitle(R.string.voice_permission_title);
                break;
            case Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED:
            case Website.MICROPHONE_AND_CAMERA_ACCESS_DENIED:
                preference.setTitle(R.string.voice_and_video_permission_title);
                break;
            case Website.INVALID_CAMERA_OR_MICROPHONE_ACCESS:
            default:
                getPreferenceScreen().removePreference(preference);
        }
        setUpListPreference(
                preference, mediaAccessType == Website.CAMERA_ACCESS_ALLOWED
                        || mediaAccessType == Website.MICROPHONE_ACCESS_ALLOWED
                        || mediaAccessType == Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED
                                ? ContentSetting.ALLOW :
                                  ContentSetting.BLOCK);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        clearStoredData();
    }

    private void clearStoredData() {
        mSite.clearAllStoredData(
                new Website.StoredDataClearedCallback() {
                    @Override
                    public void onStoredDataCleared() {
                        getPreferenceScreen().removePreference(
                                getPreferenceScreen().findPreference(PREF_CLEAR_DATA));
                        popBackIfNoSettings();
                    }
                });
    }

    private void popBackIfNoSettings() {
        if (!hasPermissionsPreferences() && !hasUsagePreferences()) {
            getActivity().finish();
        }
    }

    private void setVoiceAndVideoCaptureSetting(ContentSetting value) {
        int mediaAccessType = mSite.getMediaAccessType();
        if (value == ContentSetting.ALLOW) {
            switch (mediaAccessType) {
                case Website.CAMERA_ACCESS_DENIED:
                    mSite.setVideoCapturePermission(ContentSetting.ALLOW);
                    break;
                case Website.MICROPHONE_ACCESS_DENIED:
                    mSite.setVoiceCapturePermission(ContentSetting.ALLOW);
                    break;
                case Website.MICROPHONE_AND_CAMERA_ACCESS_DENIED:
                    mSite.setVideoCapturePermission(ContentSetting.ALLOW);
                    mSite.setVoiceCapturePermission(ContentSetting.ALLOW);
                    break;
                default:
                    assert false;
            }
        } else {
            switch (mediaAccessType) {
                case Website.CAMERA_ACCESS_ALLOWED:
                    mSite.setVideoCapturePermission(ContentSetting.BLOCK);
                    break;
                case Website.MICROPHONE_ACCESS_ALLOWED:
                    mSite.setVoiceCapturePermission(ContentSetting.BLOCK);
                    break;
                case Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED:
                    mSite.setVideoCapturePermission(ContentSetting.BLOCK);
                    mSite.setVoiceCapturePermission(ContentSetting.BLOCK);
                    break;
                default:
                    assert false;
            }
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        ContentSetting permission =
                ContentSetting.fromString((String) newValue);
        if (PREF_COOKIES_PERMISSION.equals(preference.getKey())) {
            mSite.setCookiePermission(permission);
        } else if (PREF_POPUP_PERMISSION.equals(preference.getKey())) {
            mSite.setPopupPermission(permission);
        } else if (PREF_LOCATION_ACCESS.equals(preference.getKey())) {
            mSite.setGeolocationPermission(permission);
        } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preference.getKey())) {
            setVoiceAndVideoCaptureSetting(permission);
        } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preference.getKey())) {
            mSite.setMidiPermission(permission);
        } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preference.getKey())) {
            mSite.setProtectedMediaIdentifierPermission(permission);
        } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preference.getKey())) {
            mSite.setPushNotificationPermission(permission);
        } else {
            return true;
        }

        int index = permission == ContentSetting.ALLOW ? 0 : 1;
        preference.setSummary(mListPreferenceSummaries[index]);
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        // Handle the Clear & Reset preference click by showing a confirmation.
        new AlertDialog.Builder(getActivity())
                .setTitle(R.string.website_reset)
                .setMessage(R.string.website_reset_confirmation)
                .setPositiveButton(R.string.website_reset, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        resetSite();
                    }
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
        return true;
    }

    private void resetSite() {
        mSite.setCookiePermission(null);
        mSite.setVideoCapturePermission(null);
        mSite.setVoiceCapturePermission(null);
        mSite.setPopupPermission(null);
        mSite.setGeolocationPermission(null);
        mSite.setPushNotificationPermission(null);
        mSite.setMidiPermission(null);
        mSite.setProtectedMediaIdentifierPermission(null);
        if (mSite.getTotalUsage() > 0) {
            clearStoredData();
        } else {
            // Clearing stored data implies popping back to parent menu if there
            // is nothing left to show. Therefore, we only need to implicitly
            // close the activity if there's no stored data to begin with.
            getActivity().finish();
        }
    }
}
