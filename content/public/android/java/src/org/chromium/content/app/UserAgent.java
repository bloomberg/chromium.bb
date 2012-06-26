// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.os.Build;

import org.chromium.base.CalledByNative;

/**
 * Provides necessary information for building the user agent string.
 */
class UserAgent {
    // TODO(yfriedman): Keep this reasonably up to date.
    private static final String PREVIOUS_VERSION = "4.0.3";

    @CalledByNative
    static String getUserAgentOSInfo() {
        String osInfo = "";
        final String version = Build.VERSION.RELEASE;
        if (version.length() > 0) {
            if (Character.isDigit(version.charAt(0))) {
                // Release is a version, eg "3.1"
                osInfo += version;
            } else {
                // Release doesn't have a version number yet, eg "Honeycomb"
                // In this case, use the previous release's version
                osInfo += PREVIOUS_VERSION;
            }
        } else {
            // default to "1.0"
            osInfo += "1.0";
        }
        osInfo += ";";

        if ("REL".equals(Build.VERSION.CODENAME)) {
            final String model = Build.MODEL;
            if (model.length() > 0) {
                osInfo += " " + model;
            }
        }
        final String id = Build.ID;
        if (id.length() > 0) {
            osInfo += " Build/" + id;
        }

        return osInfo;
    }
}
