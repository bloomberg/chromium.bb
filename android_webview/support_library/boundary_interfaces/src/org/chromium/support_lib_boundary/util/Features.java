// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.util;

/**
 * Class containing all the features the support library can support.
 * This class lives in the boundary interface directory so that the Android Support Library and
 * Chromium can share its definition.
 */
public class Features {
    // This class just contains constants representing features.
    private Features() {}

    // WebViewCompat.postVisualStateCallback
    public static final String VISUAL_STATE_CALLBACK = "VISUAL_STATE_CALLBACK";
}
