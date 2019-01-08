// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * As model of the keyboard accessory component, this class holds the data relevant to the visual
 * state of the accessory.
 * This includes the visibility of the accessory, its relative position and actions. Whenever the
 * state changes, it notifies its listeners - like the {@link KeyboardAccessoryMediator} or a
 * ModelChangeProcessor.
 */
class KeyboardAccessoryProperties {
    static final ReadableObjectPropertyKey<ListModel<KeyboardAccessoryData.Action>> ACTIONS =
            new ReadableObjectPropertyKey<>("actions");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableIntPropertyKey BOTTOM_OFFSET_PX =
            new WritableIntPropertyKey("bottom_offset_px");
    static final WritableBooleanPropertyKey KEYBOARD_TOGGLE_VISIBLE =
            new WritableBooleanPropertyKey("keyboard_toggle_visible");
    static final WritableObjectPropertyKey<Runnable> SHOW_KEYBOARD_CALLBACK =
            new WritableObjectPropertyKey<>("show_keyboard_callback");

    private KeyboardAccessoryProperties() {}
}
