// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome;

/**
 * Contains all of the command line switches that are specific to the chrome/
 * portion of Chromium on Android.
 */
public abstract class ChromeSwitches {
    // Switches used from Java.  Please continue switch style used Chrome where
    // options-have-hypens and are_not_split_with_underscores.

    /** Testing: pretend that the switch value is the name of a child account. */
    public static final String CHILD_ACCOUNT = "child-account";

    /** Mimic a low end device */
    public static final String ENABLE_ACCESSIBILITY_TAB_SWITCHER =
            "enable-accessibility-tab-switcher";

    /** Whether fullscreen support is disabled (auto hiding controls, etc...). */
    public static final String DISABLE_FULLSCREEN = "disable-fullscreen";

    /** Show the undo bar for high end UI devices. */
    public static final String ENABLE_HIGH_END_UI_UNDO = "enable-high-end-ui-undo";

    /** Enable toolbar swipe to change tabs in document mode */
    public static final String ENABLE_TOOLBAR_SWIPE_IN_DOCUMENT_MODE =
            "enable-toolbar-swipe-in-document-mode";

    /** Whether instant is disabled. */
    public static final String DISABLE_INSTANT = "disable-instant";

    /** Whether force-enable the "hardware acceleration" preference. */
    public static final String HARDWARE_ACCELERATION = "hardware-acceleration";

    /** If specified, enables notification center verbose logging. */
    public static final String NOTIFICATION_CENTER_LOGGING = "notification-center-logging";

    /** Enables StrictMode violation detection. By default this logs violations to logcat. */
    public static final String STRICT_MODE = "strict-mode";

    /** Don't restore persistent state from saved files on startup. */
    public static final String NO_RESTORE_STATE = "no-restore-state";

    /** Disable the First Run Experience. */
    public static final String DISABLE_FIRST_RUN_EXPERIENCE = "disable-fre";

    /** Force the crash dump to be uploaded regardless of preferences. */
    public static final String FORCE_CRASH_DUMP_UPLOAD = "force-dump-upload";

    /** Do not use OAuth2 tokens for communication with Cloud Print service for Chrome to Mobile. */
    public static final String DISABLE_CHROME_TO_MOBILE_OAUTH2 = "disable-chrome-to-mobile-oauth2";

    /** Enable debug logs for the video casting feature. */
    public static final String ENABLE_CAST_DEBUG_LOGS = "enable-cast-debug";

    /** Prevent automatic reconnection to current Cast video when Chrome restarts. */
    public static final String DISABLE_CAST_RECONNECTION = "disable-cast-reconnection";

    /** Whether site/user triggered persistent fullscreen is supported. */
    public static final String DISABLE_PERSISTENT_FULLSCREEN = "disable-persistent-fullscreen";

    /** Whether or not to enable the experimental tablet tab stack. */
    public static final String ENABLE_TABLET_TAB_STACK = "enable-tablet-tab-stack";

    /** Disables support for playing videos remotely via Android MediaRouter API. */
    public static final String DISABLE_CAST = "disable-cast";

    /** Never forward URL requests to external intents. */
    public static final String DISABLE_EXTERNAL_INTENT_REQUESTS =
            "disable-external-intent-requests";

    /** Disable document mode. */
    public static final String DISABLE_DOCUMENT_MODE = "disable-document-mode";

    /** Disable Contextual Search. */
    public static final String DISABLE_CONTEXTUAL_SEARCH = "disable-contextual-search";

    /** Enable Contextual Search. */
    public static final String ENABLE_CONTEXTUAL_SEARCH = "enable-contextual-search";

    /** Disable Contextual Search first-run flow, for testing. Not exposed to user. */
    public static final String DISABLE_CONTEXTUAL_SEARCH_PROMO_FOR_TESTING =
            "disable-contextual-search-promo-for-testing";

    /** Enable Contextual Search for instrumentation testing. Not exposed to user. */
    public static final String ENABLE_CONTEXTUAL_SEARCH_FOR_TESTING =
            "enable-contextual-search-for-testing";

    // How many thumbnails should we allow in the cache (per tab stack)?
    public static final String THUMBNAILS = "thumbnails";

    // How many "approximated" thumbnails should we allow in the cache
    // (per tab stack)?  These take very low memory but have poor quality.
    public static final String APPROXIMATION_THUMBNAILS = "approximation-thumbnails";

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Native Switches
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Sets the max number of render processes to use.
     * Native switch - content_switches::kRendererProcessLimit.
     */
    public static final String RENDER_PROCESS_LIMIT = "renderer-process-limit";

    /** Enable begin frame scheduling. */
    public static final String ENABLE_BEGIN_FRAME_SCHEDULING = "enable-begin-frame-scheduling";

    /**
     * Enable enhanced bookmarks feature.
     * Native switch - switches::kEnhancedBookmarksExperiment
     */
    public static final String ENABLE_ENHANCED_BOOKMARKS = "enhanced-bookmarks-experiment";

    /** Enable the DOM Distiller. */
    public static final String ENABLE_DOM_DISTILLER = "enable-dom-distiller";

    /** Enable experimental web-platform features, such as Push Messaging. */
    public static final String EXPERIMENTAL_WEB_PLAFTORM_FEATURES =
            "enable-experimental-web-platform-features";

    /** Enable the Reader Mode icon in toolbar. */
    public static final String ENABLE_READER_MODE_TOOLBAR_ICON = "enable-reader-mode-toolbar-icon";

    /** Enable the native app banners. */
    public static final String ENABLE_APP_INSTALL_ALERTS = "enable-app-install-alerts";

    /**
     * Use sandbox Wallet environment for requestAutocomplete.
     * Native switch - autofill::switches::kWalletServiceUseSandbox.
     */
    public static final String USE_SANDBOX_WALLET_ENVIRONMENT = "wallet-service-use-sandbox";

    /**
     * Change Google base URL.
     * Native switch - switches::kGoogleBaseURL.
     */
    public static final String GOOGLE_BASE_URL = "google-base-url";

    // Prevent instantiation.
    private ChromeSwitches() {}
}
