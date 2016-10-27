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
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

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

    @Test
    public void testSanity() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.SCOPE, SCOPE);
        bundle.putString(WebApkMetaDataKeys.NAME, NAME);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, SHORT_NAME);
        bundle.putString(WebApkMetaDataKeys.DISPLAY_MODE, DISPLAY_MODE);
        bundle.putString(WebApkMetaDataKeys.ORIENTATION, ORIENTATION);
        bundle.putString(WebApkMetaDataKeys.THEME_COLOR, THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, BACKGROUND_COLOR);
        WebApkTestHelper.registerWebApkWithMetaData(bundle);

        WebappInfo webappInfo = WebApkMetaDataUtils.extractWebappInfoFromWebApk(
                WebApkTestHelper.WEBAPK_PACKAGE_NAME, START_URL, 0);

        Assert.assertEquals(WebApkConstants.WEBAPK_ID_PREFIX + WebApkTestHelper.WEBAPK_PACKAGE_NAME,
                webappInfo.id());
        Assert.assertEquals(SCOPE, webappInfo.scopeUri().toString());
        Assert.assertEquals(NAME, webappInfo.name());
        Assert.assertEquals(SHORT_NAME, webappInfo.shortName());
        Assert.assertEquals(WebDisplayMode.MinimalUi, webappInfo.displayMode());
        Assert.assertEquals(ScreenOrientationValues.PORTRAIT, webappInfo.orientation());
        Assert.assertTrue(webappInfo.hasValidThemeColor());
        Assert.assertEquals(1L, webappInfo.themeColor());
        Assert.assertTrue(webappInfo.hasValidBackgroundColor());
        Assert.assertEquals(2L, webappInfo.backgroundColor());
        Assert.assertEquals(WebApkTestHelper.WEBAPK_PACKAGE_NAME, webappInfo.webApkPackageName());
    }

    /**
     * Test that WebappInfo are populated with the start URL passed to
     * {@link extractWebappInfoFromWebApk()} not the start URL in the WebAPK's meta data. When a
     * WebAPK is launched via a deep link from a URL within the WebAPK's scope, the WebAPK should
     * open at the URL it was deep linked from not the WebAPK's start URL.
     */
    @Test
    public void testUseStartUrlOverride() {
        String passedInStartUrl = "https://www.google.com/master_override";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(bundle);

        WebappInfo webappInfo = WebApkMetaDataUtils.extractWebappInfoFromWebApk(
                WebApkTestHelper.WEBAPK_PACKAGE_NAME, passedInStartUrl, 0);
        Assert.assertEquals(passedInStartUrl, webappInfo.uri().toString());
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
