// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.support.annotation.IntDef;
import android.text.TextUtils;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper containing a list of Downloads Home filter types and conversion methods. */
public class Filters {
    /** A list of possible filter types on offlined items. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({NONE, VIDEOS, MUSIC, IMAGES, SITES, OTHER, PREFETCHED})
    public @interface FilterType {}
    public static final int NONE = 0;
    public static final int VIDEOS = 1;
    public static final int MUSIC = 2;
    public static final int IMAGES = 3;
    public static final int SITES = 4;
    public static final int OTHER = 5;
    public static final int PREFETCHED = 6;
    public static final int FILTER_BOUNDARY = 7;

    /**
     * Converts from a {@link OfflineItem#filter} to a {@link FilterType}.  Note that not all
     * {@link OfflineItem#filter} types have a corresponding match and may return {@link #NONE}
     * as they don't correspond to any UI filter.
     *
     * @param filter The {@link OfflineItem#filter} type to convert.
     * @return       The corresponding {@link FilterType}.
     */
    public static @FilterType int fromOfflineItem(@OfflineItemFilter int filter) {
        switch (filter) {
            case OfflineItemFilter.FILTER_PAGE:
                return SITES;
            case OfflineItemFilter.FILTER_VIDEO:
                return VIDEOS;
            case OfflineItemFilter.FILTER_AUDIO:
                return MUSIC;
            case OfflineItemFilter.FILTER_IMAGE:
                return IMAGES;
            case OfflineItemFilter.FILTER_OTHER:
            case OfflineItemFilter.FILTER_DOCUMENT:
            default:
                return OTHER;
        }
    }

    /** Converts between a {@link OfflineItemFilter} and a {@link DownloadFilter.Type}. */
    public static @DownloadFilter.Type int offlineItemFilterToDownloadFilter(
            @OfflineItemFilter int filter) {
        switch (filter) {
            case OfflineItemFilter.FILTER_PAGE:
                return DownloadFilter.FILTER_PAGE;
            case OfflineItemFilter.FILTER_VIDEO:
                return DownloadFilter.FILTER_VIDEO;
            case OfflineItemFilter.FILTER_AUDIO:
                return DownloadFilter.FILTER_AUDIO;
            case OfflineItemFilter.FILTER_IMAGE:
                return DownloadFilter.FILTER_IMAGE;
            case OfflineItemFilter.FILTER_DOCUMENT:
                return DownloadFilter.FILTER_DOCUMENT;
            case OfflineItemFilter.FILTER_OTHER:
            default:
                return DownloadFilter.FILTER_OTHER;
        }
    }

    /**
     * Converts {@code filter} into a url.
     * @see DownloadFilter#getUrlForFilter(int)
     */
    public static String toUrl(@FilterType int filter) {
        if (filter == NONE) return UrlConstants.DOWNLOADS_URL;
        return UrlConstants.DOWNLOADS_FILTER_URL + filter;
    }

    /**
     * Converts {@code url} to a {@link FilterType}.
     * @see DownloadFilter#getFilterFromUrl(String)
     */
    public static @FilterType int fromUrl(String url) {
        if (TextUtils.isEmpty(url)) return NONE;
        if (!url.startsWith(UrlConstants.DOWNLOADS_FILTER_URL)) return NONE;

        @FilterType
        int filter = NONE;
        try {
            filter = Integer.parseInt(url.substring(UrlConstants.DOWNLOADS_FILTER_URL.length()));
            if (filter < 0 || filter >= FILTER_BOUNDARY) filter = NONE;
        } catch (NumberFormatException ex) {
        }

        return filter;
    }

    private Filters() {}
}