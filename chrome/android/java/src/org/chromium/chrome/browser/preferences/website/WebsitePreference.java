// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.preference.Preference;
import android.text.format.Formatter;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

/**
 * A preference that displays a website's URL and, optionally, some extra permission-related data
 * (e.g. the amount of local storage used by the site, or an icon describing what type of media
 * access the site has.)
 */
@SuppressFBWarnings("EQ_COMPARETO_USE_OBJECT_EQUALS")
class WebsitePreference extends Preference implements FaviconImageCallback {
    private final Website mSite;
    private final String mCategoryFilter;
    private final WebsiteSettingsCategoryFilter mFilter;

    private static final int TEXT_SIZE_SP = 13;

    // Loads the favicons asynchronously.
    private FaviconHelper mFaviconHelper;

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched = false;

    // Metrics for favicon processing.
    private static final int FAVICON_CORNER_RADIUS_DP = 2;
    private static final int FAVICON_SIZE_DP = 16;
    private static final int FAVICON_PADDING_DP = 4;
    private static final int FAVICON_TEXT_SIZE_SP = 10;
    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;

    WebsitePreference(Context context, Website site, String categoryFilter) {
        super(context);
        mSite = site;
        mCategoryFilter = categoryFilter;
        mFilter = new WebsiteSettingsCategoryFilter();
        setWidgetLayoutResource(R.layout.website_features);

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));

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

    @Override
    public void onFaviconAvailable(Bitmap image, String iconUrl) {
        mFaviconHelper.destroy();
        mFaviconHelper = null;
        if (image == null) {
            // Invalid favicon, produce a generic one.
            RoundedIconGenerator faviconGenerator = new RoundedIconGenerator(
                    getContext(), FAVICON_SIZE_DP, FAVICON_SIZE_DP,
                    FAVICON_CORNER_RADIUS_DP, FAVICON_BACKGROUND_COLOR,
                    FAVICON_TEXT_SIZE_SP);
            image = faviconGenerator.generateIconForUrl(faviconUrl());
        }

        setIcon(new BitmapDrawable(getContext().getResources(), image));
    }

    /**
     * Returns the url of the site to fetch a favicon for.
     */
    private String faviconUrl() {
        String origin = mSite.getAddress().getOrigin();
        if (origin == null) {
            return "http://" + mSite.getAddress().getHost();
        }

        Uri uri = Uri.parse(origin);
        if (uri.getPort() != -1) {
            // Remove the port.
            uri = uri.buildUpon().authority(uri.getHost()).build();
        }
        return uri.toString();
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
            return super.compareTo(preference);
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
        if (mFilter.showStorageSites(mCategoryFilter)) {
            long totalUsage = mSite.getTotalUsage();
            if (totalUsage > 0) {
                usageText.setText(Formatter.formatShortFileSize(getContext(), totalUsage));
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

        float density = getContext().getResources().getDisplayMetrics().density;
        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mFaviconHelper = new FaviconHelper();
            if (!mFaviconHelper.getLocalFaviconImageForURL(
                    Profile.getLastUsedProfile(), faviconUrl(),
                    FaviconHelper.FAVICON | FaviconHelper.TOUCH_ICON
                            | FaviconHelper.TOUCH_PRECOMPOSED_ICON,
                    Math.round(FAVICON_SIZE_DP * density),
                    this)) {
                onFaviconAvailable(null, null);
            }
            mFaviconFetched = true;
        }

        int iconPadding = Math.round(FAVICON_PADDING_DP * density);
        View iconView = view.findViewById(android.R.id.icon);
        iconView.setPadding(iconPadding, iconPadding, iconPadding, iconPadding);
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
}
