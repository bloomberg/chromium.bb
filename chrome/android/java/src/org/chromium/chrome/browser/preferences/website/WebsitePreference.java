// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.preference.Preference;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.R;

import java.text.DecimalFormat;
import java.text.NumberFormat;

/**
 * A preference that displays a website's URL and, optionally, some extra permission-related data
 * (e.g. the amount of local storage used by the site, or an icon describing what type of media
 * access the site has.)
 */
@SuppressFBWarnings("EQ_COMPARETO_USE_OBJECT_EQUALS")
class WebsitePreference extends Preference {
    private final Website mSite;
    private final String mCategoryFilter;
    private final WebsiteSettingsCategoryFilter mFilter;

    private static final int TEXT_SIZE_SP = 13;

    WebsitePreference(Context context, Website site, String categoryFilter) {
        super(context);
        mSite = site;
        mCategoryFilter = categoryFilter;
        mFilter = new WebsiteSettingsCategoryFilter();
        setWidgetLayoutResource(R.layout.website_features);
        refresh();
    }

    public void putSiteIntoExtras(String key) {
        getExtras().putSerializable(key, mSite);
    }

    /**
     * Return the Website this object is representing.
     */
    public Website site() {
        return mSite;
    }

    private void refresh() {
        setTitle(mSite.getTitle());
        String subtitleText = mSite.getSummary();
        if (subtitleText != null) {
            setSummary(String.format(getContext().getString(R.string.website_settings_embedded_in),
                                     subtitleText));
        }
    }

    @Override
    public int compareTo(Preference preference) {
        if (!(preference instanceof WebsitePreference)) {
            return 1;
        }
        WebsitePreference other = (WebsitePreference) preference;
        if (mFilter.showStorageSites(mCategoryFilter)) {
            return mSite.compareByStorageTo(other.mSite);
        }

        return mSite.compareByAddressTo(other.mSite);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        TextView usageText = (TextView) view.findViewById(R.id.usage_text);
        usageText.setVisibility(View.GONE);
        long totalUsage = mSite.getTotalUsage();
        if (mFilter.showStorageSites(mCategoryFilter)) {
            if (totalUsage > 0) {
                usageText.setText(sizeValueToString(getContext(), totalUsage));
                usageText.setTextSize(TEXT_SIZE_SP);
                usageText.setVisibility(View.VISIBLE);
            }
        }

        ImageView mediaCaptureIcon = (ImageView) view.findViewById(
                R.id.voice_and_video_capture_icon);
        mediaCaptureIcon.setVisibility(View.GONE);
        if (mFilter.showCameraMicSites(mCategoryFilter)) {
            int level = determineMediaIconToDisplay(mSite.getMediaAccessType());
            if (level > 0) {
                mediaCaptureIcon.setImageLevel(level);
                mediaCaptureIcon.setVisibility(View.VISIBLE);
            }
        }
    }

    /**
     * Converts type of media captured into level 0..4 to display the appropriate media icon.
     * This level is used in the drawable website_voice_and_video_capture.xml to display the icon.
     * 0 - Invalid
     * 1 - Voice and video/only video allowed - display camera allowed icon
     * 2 - Voice and video/only video denied - display camera denied icon
     * 3 - Only voice allowed - display mic allowed icon
     * 4 - Only voice denied - display mic denied icon
     */
    private static int determineMediaIconToDisplay(int mediaAccessType) {
        switch (mediaAccessType) {
            case Website.CAMERA_ACCESS_ALLOWED:
            case Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED:
                return 1;
            case Website.CAMERA_ACCESS_DENIED:
            case Website.MICROPHONE_AND_CAMERA_ACCESS_DENIED:
                return 2;
            case Website.MICROPHONE_ACCESS_ALLOWED:
                return 3;
            case Website.MICROPHONE_ACCESS_DENIED:
                return 4;
            default:
                return 0;
        }
    }

    static String sizeValueToString(Context context, long bytes) {
        final String label[] = {
            context.getString(R.string.origin_settings_storage_bytes),
            context.getString(R.string.origin_settings_storage_kbytes),
            context.getString(R.string.origin_settings_storage_mbytes),
            context.getString(R.string.origin_settings_storage_gbytes),
            context.getString(R.string.origin_settings_storage_tbytes),
        };

        if (bytes <= 0) {
            return NumberFormat.getIntegerInstance().format(0) + " " + label[0];
        }

        int i = 0;
        float size = bytes;
        for (i = 0; i < label.length; ++i) {
            if (size < 1024 || i == label.length - 1)
                break;
            size /= 1024.0F;
        }

        DecimalFormat formatter = new DecimalFormat("#.## ");
        return formatter.format(size) + label[i];
    }
}
