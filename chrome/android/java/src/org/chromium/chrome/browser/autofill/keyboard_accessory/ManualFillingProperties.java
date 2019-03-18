// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;

import android.support.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties defined here reflect the visible state of the ManualFilling-components.
 */
class ManualFillingProperties {
    static final PropertyModel.WritableBooleanPropertyKey SHOW_WHEN_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("show_when_visible");
    static final PropertyModel.WritableIntPropertyKey KEYBOARD_EXTENSION_STATE =
            new PropertyModel.WritableIntPropertyKey("keyboard_extension_state");

    @IntDef({HIDDEN, EXTENDING_KEYBOARD, REPLACING_KEYBOARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface KeyboardExtensionState {
        int HIDDEN = 0;
        int EXTENDING_KEYBOARD = 1;
        int REPLACING_KEYBOARD = 2;
    }

    static PropertyModel createFillingModel() {
        return new PropertyModel.Builder(SHOW_WHEN_VISIBLE, KEYBOARD_EXTENSION_STATE)
                .with(SHOW_WHEN_VISIBLE, false)
                .with(KEYBOARD_EXTENSION_STATE, HIDDEN)
                .build();
    }

    private ManualFillingProperties() {}
}
