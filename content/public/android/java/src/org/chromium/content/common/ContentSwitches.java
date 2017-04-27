// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

/**
 * Contains all of the command line switches that are specific to the content/
 * portion of Chromium on Android.
 */
public abstract class ContentSwitches {
    // Tell Java to use the official command line, loaded from the
    // official-command-line.xml files.  WARNING this is not done
    // immediately on startup, so early running Java code will not see
    // these flags.
    public static final String ADD_OFFICIAL_COMMAND_LINE = "add-official-command-line";

    // Enables test intent handling.
    public static final String ENABLE_TEST_INTENTS = "enable-test-intents";

    // Dump frames-per-second to the log
    public static final String LOG_FPS = "log-fps";

    // Whether Chromium should use a mobile user agent.
    public static final String USE_MOBILE_UA = "use-mobile-user-agent";

    // Change the url of the JavaScript that gets injected when accessibility mode is enabled.
    public static final String ACCESSIBILITY_JAVASCRIPT_URL = "accessibility-js-url";

    // Sets the ISO country code that will be used for phone number detection.
    public static final String NETWORK_COUNTRY_ISO = "network-country-iso";

    // How much of the browser controls need to be shown before they will auto show.
    public static final String TOP_CONTROLS_SHOW_THRESHOLD = "top-controls-show-threshold";

    // How much of the browser controls need to be hidden before they will auto hide.
    public static final String TOP_CONTROLS_HIDE_THRESHOLD = "top-controls-hide-threshold";

    // Native switch - shell_switches::kRunLayoutTest
    public static final String RUN_LAYOUT_TEST = "run-layout-test";

    // Native switch - chrome_switches::kDisablePopupBlocking
    public static final String DISABLE_POPUP_BLOCKING = "disable-popup-blocking";

    // Native switch kEnableCredentialManagerAPI
    public static final String ENABLE_CREDENTIAL_MANAGER_API = "enable-credential-manager-api";

    // Native switch kDisableGestureRequirementForMediaPlayback
    public static final String DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK =
            "disable-gesture-requirement-for-media-playback";

    // Native switch kDisableGestureRequirementForPresentation
    public static final String DISABLE_GESTURE_REQUIREMENT_FOR_PRESENTATION =
            "disable-gesture-requirement-for-presentation";

    // Native switch kRendererProcessLimit
    public static final String RENDER_PROCESS_LIMIT = "renderer-process-limit";

    // Native switch kInProcessGPU
    public static final String IN_PROCESS_GPU = "in-process-gpu";

    // Native switch kProcessType
    public static final String SWITCH_PROCESS_TYPE = "type";

    // Native switch kRendererProcess
    public static final String SWITCH_RENDERER_PROCESS = "renderer";

    // Native switch kUtilityProcess
    public static final String SWITCH_UTILITY_PROCESS = "utility";

    // Native switch kGPUProcess
    public static final String SWITCH_GPU_PROCESS = "gpu-process";

    // Use fake device for Media Stream to replace actual camera and microphone.
    public static final String USE_FAKE_DEVICE_FOR_MEDIA_STREAM =
            "use-fake-device-for-media-stream";

    // Prevent instantiation.
    private ContentSwitches() {}

    public static String getSwitchValue(final String[] commandLine, String switchKey) {
        if (commandLine == null || switchKey == null) {
            return null;
        }
        // This format should be matched with the one defined in command_line.h.
        final String switchKeyPrefix = "--" + switchKey + "=";
        for (String command : commandLine) {
            if (command != null && command.startsWith(switchKeyPrefix)) {
                return command.substring(switchKeyPrefix.length());
            }
        }
        return null;
    }
}
