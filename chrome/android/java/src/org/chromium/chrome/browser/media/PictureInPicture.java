// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.AppOpsManager;
import android.content.Context;
import android.os.Build;

/**
 * Utility for determining if Picture-in-Picture is available and whether the user has disabled
 * Picture-in-Picture for Chrome using the system's per-application settings.
 */
public abstract class PictureInPicture {
    private PictureInPicture() {}

    /**
     * Determines whether Picture-is-Picture is enabled for the app represented by |context|.
     * Picture-in-Picture may be disabled because either the user, or a management tool, has
     * explicitly disallowed the Chrome App to enter Picture-in-Picture.
     *
     * @param context The context to check of whether it can enter Picture-in-Picture.
     * @return boolean true if Picture-In-Picture is enabled, otherwise false.
     */
    public static boolean isEnabled(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }

        final AppOpsManager appOpsManager =
                (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
        final int status = appOpsManager.checkOpNoThrow(AppOpsManager.OPSTR_PICTURE_IN_PICTURE,
                context.getApplicationInfo().uid, context.getPackageName());

        return (status == AppOpsManager.MODE_ALLOWED);
    }
}
