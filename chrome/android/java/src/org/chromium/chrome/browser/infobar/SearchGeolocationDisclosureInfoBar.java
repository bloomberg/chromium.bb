// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * An infobar to disclose to the user that the default search engine has geolocation access by
 * default.
 */
public class SearchGeolocationDisclosureInfoBar extends InfoBar {
    @CalledByNative
    private static InfoBar show(int enumeratedIconId, String message) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        return new SearchGeolocationDisclosureInfoBar(drawableId, message);
    }

    /**
     * Creates the infobar.
     * @param iconDrawableId Drawable ID corresponding to the icon that the infobar will show.
     * @param messageText Message to display in the infobar.
     */
    private SearchGeolocationDisclosureInfoBar(int iconDrawableId, String messageText) {
        super(iconDrawableId, null, messageText);
    }
}
