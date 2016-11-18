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

import java.util.Map;

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
     * Test that {@link WebApkMetaDataUtils.getIconUrlAndIconMurmur2HashMap} can read multiple icon
     * URLs and multiple icon murmur2 hashes from metadata and returns a map with
     * {icon URL, icon hash} pairs.
     */
    @Test
    public void testGetIconUrlAndMurmur2HashFromMetaData() {
        String[] iconUrls = {"/icon1.png", "/icon2.png"};
        String[] hashes = {"1", "2"};

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                getIconUrlsAndIconMurmur2HashesString(iconUrls, hashes));
        Map<String, String> iconUrlAndIconMurmur2HashMap =
                WebApkMetaDataUtils.getIconUrlAndIconMurmur2HashMap(bundle);

        Assert.assertEquals(iconUrls.length, iconUrlAndIconMurmur2HashMap.keySet().size());
        Assert.assertEquals(hashes[0], iconUrlAndIconMurmur2HashMap.get(iconUrls[0]));
        Assert.assertEquals(hashes[1], iconUrlAndIconMurmur2HashMap.get(iconUrls[1]));
    }

    /**
     * WebApkIconHasher generates hashes with values [0, 2^64-1]. 2^64-1 is greater than
     * {@link Long#MAX_VALUE}. Test that {@link #getIconUrlAndIconMurmur2HashMap()} can read a hash
     * with value 2^64 - 1.
     */
    @Test
    public void testGetIconMurmur2HashFromMetaData() {
        String[] iconUrls = {"/icon1.png"};
        String[] hashes = {"18446744073709551615"}; // 2^64 - 1

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                getIconUrlsAndIconMurmur2HashesString(iconUrls, hashes));
        Map<String, String> iconUrlAndIconMurmur2HashMap =
                WebApkMetaDataUtils.getIconUrlAndIconMurmur2HashMap(bundle);

        Assert.assertEquals(1, iconUrlAndIconMurmur2HashMap.size());
        Assert.assertEquals(hashes[0], iconUrlAndIconMurmur2HashMap.get(iconUrls[0]));
    }

    /**
     * Create a WebAPK's icon URLs and icon murmur2 hashes metadata tag from the give URLs and
     * hashes.
     */
    private String getIconUrlsAndIconMurmur2HashesString(String[] iconUrls,
            String[] iconMurmur2Hahes) {
        final String separator = " ";
        StringBuilder iconUrlsAndIconMurmur2Hahses = new StringBuilder();
        for (int i = 0; i < iconUrls.length; i++) {
            iconUrlsAndIconMurmur2Hahses.append(iconUrls[i]);
            iconUrlsAndIconMurmur2Hahses.append(separator);
            iconUrlsAndIconMurmur2Hahses.append(iconMurmur2Hahes[i]);
            if (i < iconUrls.length - 1) {
                iconUrlsAndIconMurmur2Hahses.append(separator);
            }
        }
        return iconUrlsAndIconMurmur2Hahses.toString();
    }
}
