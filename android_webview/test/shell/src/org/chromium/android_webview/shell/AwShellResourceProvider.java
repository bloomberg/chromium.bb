// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.content.Context;

import org.chromium.android_webview.AwResource;

/**
 * Registers resources for the Android webview shell.
 */
public class AwShellResourceProvider {
    private static boolean sInitialized;

    public static void registerResources(Context context) {
        if (sInitialized) {
            return;
        }

        AwResource.setResources(context.getResources());

        AwResource.setErrorPageResources(R.raw.error, R.raw.blank_html);

        AwResource.setDefaultTextEncoding(R.string.default_encoding);

        AwResource.setConfigKeySystemUuidMapping(R.array.config_key_system_uuid_mapping);

        sInitialized = true;
    }
}
