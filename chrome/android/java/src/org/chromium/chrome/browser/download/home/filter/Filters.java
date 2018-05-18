// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A list of possible filter types on offlined items. */
public interface Filters {
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
}