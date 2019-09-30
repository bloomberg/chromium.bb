// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains the properties that an add-to-homescreen {@link PropertyModel} can have.
 */
public class AddToHomescreenProperties {
    @IntDef({AppType.NATIVE, AppType.WEB_APK, AppType.SHORTCUT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AppType {
        int NATIVE = 0;
        int WEB_APK = 1;
        int SHORTCUT = 2;
    }

    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Bitmap, Boolean>> ICON =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey TYPE =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey CAN_SUBMIT =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> NATIVE_INSTALL_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableFloatPropertyKey NATIVE_APP_RATING =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
            TITLE, URL, ICON, TYPE, CAN_SUBMIT, NATIVE_INSTALL_BUTTON_TEXT, NATIVE_APP_RATING};
}
