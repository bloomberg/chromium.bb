// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.design.widget.TabLayout;

import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * These properties are used to describe a model for the tab layout component as used in the
 * keyboard accessory. The properties are describing all known tabs.
 */
class KeyboardAccessoryTabLayoutProperties {
    static final ReadableObjectPropertyKey<ListModel<KeyboardAccessoryData.Tab>> TABS =
            new ReadableObjectPropertyKey<>();
    static final /* @Nullable */ WritableObjectPropertyKey<Integer> ACTIVE_TAB =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<TabLayout.OnTabSelectedListener>
            TAB_SELECTION_CALLBACKS = new WritableObjectPropertyKey<>();

    private KeyboardAccessoryTabLayoutProperties() {}
}
