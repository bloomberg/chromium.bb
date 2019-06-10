// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * UI property-model based encapsulating class for important site suggestion data.
 */
class SiteSuggestionModel {
    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.ReadableObjectPropertyKey<String> URL_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.ReadableIntPropertyKey SOURCE_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    static final PropertyModel.ReadableObjectPropertyKey<String> WHITELIST_ICON_PATH_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<Bitmap> ICON_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.ReadableObjectPropertyKey<Bitmap> DEFAULT_ICON_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();

    static PropertyModel getSiteSuggestionModel(SiteSuggestion suggestion, Bitmap defaultIcon) {
        String title = UrlUtilities.getDomainAndRegistry(suggestion.url, false);
        if (TextUtils.isEmpty(title)) {
            // Fallback to title.
            title = suggestion.title;
        }
        return new PropertyModel
                .Builder(TITLE_KEY, URL_KEY, ICON_KEY, SOURCE_KEY, WHITELIST_ICON_PATH_KEY,
                        DEFAULT_ICON_KEY)
                .with(TITLE_KEY, title)
                .with(URL_KEY, suggestion.url)
                .with(SOURCE_KEY, suggestion.source)
                .with(WHITELIST_ICON_PATH_KEY, suggestion.whitelistIconPath)
                .with(DEFAULT_ICON_KEY, defaultIcon)
                .build();
    }
}
