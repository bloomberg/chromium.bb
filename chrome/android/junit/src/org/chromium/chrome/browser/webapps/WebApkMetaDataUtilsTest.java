// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

/**
 * Tests WebApkMetaDataUtils.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkMetaDataUtilsTest {

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";
    private static final String SCOPE = "https://www.google.com/scope";
    private static final String NAME = "name";
    private static final String SHORT_NAME = "short_name";
    private static final String DISPLAY_MODE = "minimal-ui";
    private static final String ORIENTATION = "portrait";
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
    }

    /**
     * WebApkIconHasher generates hashes with values [0, 2^64-1]. 2^64-1 is greater than
     * {@link Long#MAX_VALUE}. Test that {@link #getIconMurmur2HashFromMetaData()} can read a hash
     * with value 2^64 - 1.
     */
    @Test
    public void testGetIconMurmur2HashFromMetaData() {
        String hash = "18446744073709551615"; // 2^64 - 1
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.ICON_MURMUR2_HASH, hash + "L");
        String extractedHash = WebApkMetaDataUtils.getIconMurmur2HashFromMetaData(bundle);
        Assert.assertEquals(hash, extractedHash);
    }
}
