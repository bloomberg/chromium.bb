// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;

import java.util.Locale;

/**
 * Constructs a User-Agent string.
 */
public final class UserAgent {
    private static final Object sLock = new Object();

    private static int sVersionCode;

    private UserAgent() {
    }

    public static String from(Context context) {
        StringBuilder builder = new StringBuilder();

        // Our package name and version.
        builder.append(context.getPackageName());
        builder.append('/');
        builder.append(versionFromContext(context));

        // The platform version.
        builder.append(" (Linux; U; Android ");
        builder.append(Build.VERSION.RELEASE);
        builder.append("; ");
        builder.append(Locale.getDefault().toString());

        String model = Build.MODEL;
        if (model.length() > 0) {
            builder.append("; ");
            builder.append(model);
        }

        String id = Build.ID;
        if (id.length() > 0) {
            builder.append("; Build/");
            builder.append(id);
        }

        builder.append("; Cronet/");
        builder.append(Version.CRONET_VERSION);

        builder.append(')');

        return builder.toString();
    }

    private static int versionFromContext(Context context) {
        synchronized (sLock) {
            if (sVersionCode == 0) {
                PackageManager packageManager = context.getPackageManager();
                String packageName = context.getPackageName();
                try {
                    PackageInfo packageInfo = packageManager.getPackageInfo(
                            packageName, 0);
                    sVersionCode = packageInfo.versionCode;
                } catch (NameNotFoundException e) {
                    throw new IllegalStateException(
                            "Cannot determine package version");
                }
            }
            return sVersionCode;
        }
    }
}
