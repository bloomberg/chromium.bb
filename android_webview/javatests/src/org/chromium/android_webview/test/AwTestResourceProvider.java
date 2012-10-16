// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;

import org.chromium.android_webview.AwResource;
import org.chromium.content.app.AppResource;

public class AwTestResourceProvider {
    private static boolean sInitialized;

    static void registerResources(Context context) {
        if (sInitialized) {
            return;
        }

        AppResource.DRAWABLE_ICON_ACTION_BAR_SHARE = R.drawable.resource_icon;
        AppResource.DRAWABLE_ICON_ACTION_BAR_WEB_SEARCH = R.drawable.resource_icon;

        AppResource.STRING_ACTION_BAR_SHARE = R.string.test_string;
        AppResource.STRING_ACTION_BAR_WEB_SEARCH = R.string.test_string;

        AwResource.setResources(context.getResources());

        AwResource.RAW_LOAD_ERROR = R.raw.blank_html;
        AwResource.RAW_NO_DOMAIN = R.raw.blank_html;

        AwResource.STRING_DEFAULT_TEXT_ENCODING = R.string.test_string;

        sInitialized = true;
    }
}
