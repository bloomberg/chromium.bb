// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * List of properties to designate information about a single tab.
 */
public class TabProperties {
    public static final PropertyModel.ReadableIntPropertyKey TAB_ID = new ReadableIntPropertyKey();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_SELECTED_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_CLOSED_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Bitmap> FAVICON =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.ThumbnailFetcher>
            THUMBNAIL_FETCHER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS_TAB_GRID =
            new PropertyKey[] {TAB_ID, TAB_SELECTED_LISTENER, TAB_CLOSED_LISTENER, FAVICON,
                    THUMBNAIL_FETCHER, TITLE, IS_SELECTED};

    public static final PropertyKey[] ALL_KEYS_TAB_STRIP = new PropertyKey[] {
            TAB_ID, TAB_SELECTED_LISTENER, TAB_CLOSED_LISTENER, FAVICON, IS_SELECTED, TITLE};
}
