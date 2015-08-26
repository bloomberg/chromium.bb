// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests the WebappInfo class's ability to parse various URLs.
 */
public class WebappInfoTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Webapps"})
    public void testAbout() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        WebappInfo info = WebappInfo.create(id, url, null, name, shortName,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        assertNotNull(info);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testRandomUrl() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://google.com";

        WebappInfo info = WebappInfo.create(id, url, null, name, shortName,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        assertNotNull(info);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testSpacesInUrl() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String bustedUrl = "http://money.cnn.com/?category=Latest News";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, bustedUrl);

        WebappInfo info = WebappInfo.create(intent);
        assertNotNull(info);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentTitleFallBack() {
        String id = "webapp id";
        String title = "webapp title";
        String url = "about:blank";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(title, info.name());
        assertEquals(title, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentNameBlankNoTitle() {
        String id = "webapp id";
        String shortName = "name";
        String url = "about:blank";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals("", info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentShortNameFallBack() {
        String id = "webapp id";
        String title = "webapp title";
        String shortName = "name";
        String url = "about:blank";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(title, info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentNameShortname() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(name, info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testOrientationAndSource() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://money.cnn.com";

        WebappInfo info = WebappInfo.create(id, url, null, name, shortName,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        assertEquals(ScreenOrientationValues.DEFAULT, info.orientation());
        assertEquals(ShortcutSource.UNKNOWN, info.source());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testNormalColors() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://money.cnn.com";
        long themeColor = 0xFF00FF00L;
        long backgroundColor = 0xFF0000FFL;

        WebappInfo info = WebappInfo.create(id, url, null, name, shortName,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                themeColor, backgroundColor);
        assertEquals(info.themeColor(), themeColor);
        assertEquals(info.backgroundColor(), backgroundColor);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testInvalidOrMissingColors() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://money.cnn.com";

        WebappInfo info = WebappInfo.create(id, url, null, name, shortName,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        assertEquals(info.themeColor(), ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        assertEquals(info.backgroundColor(), ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testColorsIntentCreation() {
        String id = "webapp id";
        String url = "http://money.cnn.com";
        long themeColor = 0xFF00FF00L;
        long backgroundColor = 0xFF0000FFL;

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, themeColor);
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor);
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(info.themeColor(), themeColor);
        assertEquals(info.backgroundColor(), backgroundColor);
    }
}
