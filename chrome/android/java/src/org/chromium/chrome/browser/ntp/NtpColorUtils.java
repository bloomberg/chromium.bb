// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.res.Resources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Utility class for figuring out which colors to use for the NTP. This class is needed while we
 * transition the NTP to the new material design spec.
 */
public class NtpColorUtils {

    private NtpColorUtils() {}

    public static int getBackgroundColorResource(Resources res) {
        return (ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SNIPPETS)
                       || ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_MATERIAL_DESIGN))
                ? ApiCompatibilityUtils.getColor(res, R.color.ntp_material_design_bg)
                : ApiCompatibilityUtils.getColor(res, R.color.ntp_bg);
    }
}
