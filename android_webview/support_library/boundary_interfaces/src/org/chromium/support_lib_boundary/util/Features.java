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

    // WebViewCompat#postVisualStateCallback
    // WebViewClientCompat#onPageCommitVisible
    public static final String VISUAL_STATE_CALLBACK = "VISUAL_STATE_CALLBACK";

    // WebViewClientCompat#onReceivedError(WebView, WebResourceRequest, WebResourceError)
    public static final String WEB_RESOURCE_ERROR = "WEB_RESOURCE_ERROR";

    // WebViewClientCompat#onReceivedHttpError
    public static final String RECEIVE_HTTP_ERROR = "RECEIVE_HTTP_ERROR";

    // WebViewClientCompat#onSafeBrowsingHit
    public static final String SAFE_BROWSING_HIT = "SAFE_BROWSING_HIT";

    // WebViewClientCompat#shouldOverrideUrlLoading
    public static final String SHOULD_OVERRIDE_WITH_REDIRECTS = "SHOULD_OVERRIDE_WITH_REDIRECTS";
}
