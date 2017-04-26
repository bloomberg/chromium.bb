// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Context;

import com.google.vr.testframework.controller.ControllerTestApi;

/**
 * Wrapper for the ControllerTestApi class to handle more complex actions such
 * as clicking and dragging.
 *
 * Requires that VrCore's settings file is modified to use the test API:
 *   - UseAutomatedController: true
 *   - PairedControllerDriver: "DRIVER_AUTOMATED"
 *   - PairedControllerAddress: "FOO"
 */
public class EmulatedVrController {
    private final ControllerTestApi mApi;

    public EmulatedVrController(Context context) {
        mApi = new ControllerTestApi(context);
    }

    public ControllerTestApi getApi() {
        return mApi;
    }

    /**
     * Presses and quickly releases the Daydream controller's touchpad button.
     * Or, if the button is already pressed, releases and quickly presses again.
     */
    public void pressReleaseTouchpadButton() {
        mApi.buttonEvent.sendClickButtonEvent();
    }

    // TODO(bsheedy): Add support for more complex actions, e.g. click/drag/release
}
