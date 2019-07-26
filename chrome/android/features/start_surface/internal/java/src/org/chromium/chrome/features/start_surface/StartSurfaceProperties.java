// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the start surface properties. */
class StartSurfaceProperties {
    public static interface BottomBarClickListener {
        /**
         * Called when clicks on the home button.
         * */
        void onHomeButtonClicked();

        /**
         * Called when clicks on the explore button.
         * */
        void onExploreButtonClicked();
    }
    public static final PropertyModel
            .WritableObjectPropertyKey<BottomBarClickListener> BottomBarClickListener =
            new PropertyModel.WritableObjectPropertyKey<BottomBarClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_EXPLORE_SURFACE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SHOWING_OVERVIEW =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            BottomBarClickListener, IS_EXPLORE_SURFACE_VISIBLE, IS_INCOGNITO, IS_SHOWING_OVERVIEW};
}