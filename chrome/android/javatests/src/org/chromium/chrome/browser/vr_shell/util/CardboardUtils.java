// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.browser.vr_shell.VrTestFramework;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * Class containing utility functions for sending Cardboard clicks, aka
 * screen taps.
 */
public class CardboardUtils {
    /**
     * Use to simulate a cardboard click by sending a touch event to the device.
     * @param activity The activity to send the cardboard click to.
     */
    public static void sendCardboardClick(Activity activity) {
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                activity.getWindow().getDecorView().getRootView());
    }

    /**
     * Sends a cardboard click then waits for the JavaScript step to finish.
     *
     * Only meant to be used alongside the test framework from VrTestFramework.
     * @param activity The activity to send the cardboard click to.
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public static void sendCardboardClickAndWait(Activity activity, WebContents webContents) {
        sendCardboardClick(activity);
        VrTestFramework.waitOnJavaScriptStep(webContents);
    }
}
