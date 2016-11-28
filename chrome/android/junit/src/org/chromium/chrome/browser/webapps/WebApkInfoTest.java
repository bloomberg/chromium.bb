// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.Map;

/**
 * Tests WebApkInfo.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkInfoTest {

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";
    private static final String SCOPE = "https://www.google.com/scope";
    private static final String NAME = "name";
    private static final String SHORT_NAME = "short_name";
    private static final String DISPLAY_MODE = "minimal-ui";
    private static final String ORIENTATION = "portrait";
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";
    private static final int SHELL_APK_VERSION = 3;
    private static final String MANIFEST_URL = "https://www.google.com/alphabet.json";
    private static final String ICON_URL = "https://www.google.com/scope/worm.png";
    private static final String ICON_MURMUR2_HASH = "5";
    private static final int SOURCE = ShortcutSource.NOTIFICATION;

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
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, SHELL_APK_VERSION);
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, MANIFEST_URL);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_URL, ICON_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_MURMUR2_HASH, ICON_MURMUR2_HASH + "L");
        WebApkTestHelper.registerWebApkWithMetaData(bundle);

        Intent intent = new Intent();
        intent.putExtra(
                ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);

        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(WebApkConstants.WEBAPK_ID_PREFIX + WebApkTestHelper.WEBAPK_PACKAGE_NAME,
                info.id());
        Assert.assertEquals(SCOPE, info.scopeUri().toString());
        Assert.assertEquals(NAME, info.name());
        Assert.assertEquals(SHORT_NAME, info.shortName());
        Assert.assertEquals(WebDisplayMode.MinimalUi, info.displayMode());
        Assert.assertEquals(ScreenOrientationValues.PORTRAIT, info.orientation());
        Assert.assertTrue(info.hasValidThemeColor());
        Assert.assertEquals(1L, info.themeColor());
        Assert.assertTrue(info.hasValidBackgroundColor());
        Assert.assertEquals(2L, info.backgroundColor());
        Assert.assertEquals(WebApkTestHelper.WEBAPK_PACKAGE_NAME, info.webApkPackageName());
        Assert.assertEquals(SHELL_APK_VERSION, info.shellApkVersion());
        Assert.assertEquals(MANIFEST_URL, info.manifestUrl());
        Assert.assertEquals(START_URL, info.manifestStartUrl());

        Assert.assertEquals(1, info.iconUrlToMurmur2HashMap().size());
        Assert.assertTrue(info.iconUrlToMurmur2HashMap().containsKey(ICON_URL));
        Assert.assertEquals(ICON_MURMUR2_HASH, info.iconUrlToMurmur2HashMap().get(ICON_URL));

        Assert.assertEquals(SOURCE, info.source());
    }

    /**
     * Test that {@link WebApkInfo#create()} populates {@link WebApkInfo#uri()} with the start URL
     * from the intent not the start URL in the WebAPK's meta data. When a WebAPK is launched via a
     * deep link from a URL within the WebAPK's scope, the WebAPK should open at the URL it was deep
     * linked from not the WebAPK's start URL.
     */
    @Test
    public void testUseStartUrlOverride() {
        String intentStartUrl = "https://www.google.com/master_override";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(bundle);

        Intent intent = new Intent();
        intent.putExtra(
                ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, intentStartUrl);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(intentStartUrl, info.uri().toString());

        // {@link WebApkInfo#manifestStartUrl()} should contain the start URL from the Android
        // Manifest.
        Assert.assertEquals(START_URL, info.manifestStartUrl());
    }

    /**
     * Test that {@link WebApkInfo#create} can read multiple icon URLs and multiple icon murmur2
     * hashes from the WebAPK's meta data.
     */
    @Test
    public void testGetIconUrlAndMurmur2HashFromMetaData() {
        String iconUrl1 = "/icon1.png";
        String murmur2Hash1 = "1";
        String iconUrl2 = "/icon2.png";
        String murmur2Hash2 = "2";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                iconUrl1 + " " + murmur2Hash1 + " " + iconUrl2 + " " + murmur2Hash2);
        WebApkTestHelper.registerWebApkWithMetaData(bundle);
        Intent intent = new Intent();
        intent.putExtra(
                ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(2, iconUrlToMurmur2HashMap.size());
        Assert.assertEquals(murmur2Hash1, iconUrlToMurmur2HashMap.get(iconUrl1));
        Assert.assertEquals(murmur2Hash2, iconUrlToMurmur2HashMap.get(iconUrl2));
    }

    /**
     * WebApkIconHasher generates hashes with values [0, 2^64-1]. 2^64-1 is greater than
     * {@link Long#MAX_VALUE}. Test that {@link WebApkInfo#create()} can read a hash with value
     * 2^64 - 1.
     */
    @Test
    public void testGetIconMurmur2HashFromMetaData() {
        String hash = "18446744073709551615"; // 2^64 - 1

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES, "randomUrl " + hash);
        WebApkTestHelper.registerWebApkWithMetaData(bundle);
        Intent intent = new Intent();
        intent.putExtra(
                ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(1, iconUrlToMurmur2HashMap.size());
        Assert.assertTrue(iconUrlToMurmur2HashMap.containsValue(hash));
    }
}
