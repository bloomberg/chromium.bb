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
    public static final String RECEIVE_WEB_RESOURCE_ERROR = "RECEIVE_WEB_RESOURCE_ERROR";

    // WebViewClientCompat#onReceivedHttpError
    public static final String RECEIVE_HTTP_ERROR = "RECEIVE_HTTP_ERROR";

    // WebViewClientCompat#onSafeBrowsingHit
    public static final String SAFE_BROWSING_HIT = "SAFE_BROWSING_HIT";

    // WebViewClientCompat#shouldOverrideUrlLoading
    public static final String SHOULD_OVERRIDE_WITH_REDIRECTS = "SHOULD_OVERRIDE_WITH_REDIRECTS";

    // WebSettingsCompat.getOffscreenPreRaster
    // WebSettingsCompat.setOffscreenPreRaster
    public static final String OFF_SCREEN_PRERASTER = "OFF_SCREEN_PRERASTER";

    // WebSettingsCompat.getSafeBrowsingEnabled
    // WebSettingsCompat.setSafeBrowsingEnabled
    public static final String SAFE_BROWSING_ENABLE = "SAFE_BROWSING_ENABLE";

    // WebSettingsCompat.getDisabledActionModeMenuItems
    // WebSettingsCompat.setDisabledActionModeMenuItems
    public static final String DISABLED_ACTION_MODE_MENU_ITEMS = "DISABLED_ACTION_MODE_MENU_ITEMS";

    // WebViewCompat.startSafeBrowsing
    public static final String START_SAFE_BROWSING = "START_SAFE_BROWSING";

    // WebViewCompat.setSafeBrowsingWhitelist
    public static final String SAFE_BROWSING_WHITELIST = "SAFE_BROWSING_WHITELIST";

    // WebViewCompat.getSafeBrowsingPrivacyPolicyUrl
    public static final String SAFE_BROWSING_PRIVACY_POLICY_URL =
            "SAFE_BROWSING_PRIVACY_POLICY_URL";

    // ServiceWorkerControllerCompat.getInstance
    // ServiceWorkerControllerCompat.getServiceWorkerWebSettings
    // ServiceWorkerControllerCompat.setServiceWorkerClient
    public static final String SERVICE_WORKER_BASIC_USAGE = "SERVICE_WORKER_BASIC_USAGE";

    // ServiceWorkerClientCompat.shouldInterceptRequest
    public static final String SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST =
            "SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST";

    // ServiceWorkerWebSettingsCompat.getCacheMode
    // ServiceWorkerWebSettingsCompat.setCacheMode
    public static final String SERVICE_WORKER_CACHE_MODE = "SERVICE_WORKER_CACHE_MODE";

    // ServiceWorkerWebSettingsCompat.getAllowContentAccess
    // ServiceWorkerWebSettingsCompat.setAllowContentAccess
    public static final String SERVICE_WORKER_CONTENT_ACCESS = "SERVICE_WORKER_CONTENT_ACCESS";

    // ServiceWorkerWebSettingsCompat.getAllowFileAccess
    // ServiceWorkerWebSettingsCompat.setAllowFileAccess
    public static final String SERVICE_WORKER_FILE_ACCESS = "SERVICE_WORKER_FILE_ACCESS";

    // ServiceWorkerWebSettingsCompat.getBlockNetworkLoads
    // ServiceWorkerWebSettingsCompat.setBlockNetworkLoads
    public static final String SERVICE_WORKER_BLOCK_NETWORK_LOADS =
            "SERVICE_WORKER_BLOCK_NETWORK_LOADS";

    // WebResourceRequest.isRedirect
    public static final String WEB_RESOURCE_REQUEST_IS_REDIRECT =
            "WEB_RESOURCE_REQUEST_IS_REDIRECT";

    // WebResourceError.getDescription
    public static final String WEB_RESOURCE_ERROR_GET_DESCRIPTION =
            "WEB_RESOURCE_ERROR_GET_DESCRIPTION";

    // WebResourceError.getErrorCode
    public static final String WEB_RESOURCE_ERROR_GET_CODE = "WEB_RESOURCE_ERROR_GET_CODE";

    // SafeBrowsingResponse.backToSafety
    public static final String SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY =
            "SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY";

    // SafeBrowsingResponse.proceed
    public static final String SAFE_BROWSING_RESPONSE_PROCEED = "SAFE_BROWSING_RESPONSE_PROCEED";

    // SafeBrowsingResponse.showInterstitial
    public static final String SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL =
            "SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL";
}
