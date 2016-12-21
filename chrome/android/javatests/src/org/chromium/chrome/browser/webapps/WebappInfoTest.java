// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.platform.WebDisplayMode;
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

        WebappInfo info = WebappInfo.create(id, url, null, null, name, shortName,
                WebDisplayMode.Standalone, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        assertNotNull(info);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testRandomUrl() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://google.com";

        WebappInfo info = WebappInfo.create(id, url, null, null, name, shortName,
                WebDisplayMode.Standalone, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
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
        String title = "webapp title";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(title, info.name());
        assertEquals(title, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentNameBlankNoTitle() {
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals("", info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentShortNameFallBack() {
        String title = "webapp title";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(title, info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentNameShortname() {
        String name = "longName";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(name, info.name());
        assertEquals(shortName, info.shortName());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testDisplayModeAndOrientationAndSource() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://money.cnn.com";

        WebappInfo info = WebappInfo.create(id, url, null, null, name, shortName,
                WebDisplayMode.Fullscreen, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        assertEquals(WebDisplayMode.Fullscreen, info.displayMode());
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

        WebappInfo info = WebappInfo.create(id, url, null, null, name, shortName,
                WebDisplayMode.Standalone, ScreenOrientationValues.DEFAULT,
                ShortcutSource.UNKNOWN, themeColor, backgroundColor, false);
        assertEquals(themeColor, info.themeColor());
        assertEquals(backgroundColor, info.backgroundColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testInvalidOrMissingColors() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://money.cnn.com";

        WebappInfo info = WebappInfo.create(id, url, null, null, name, shortName,
                WebDisplayMode.Standalone, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        assertEquals(ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, info.themeColor());
        assertEquals(ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, info.backgroundColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testColorsIntentCreation() {
        long themeColor = 0xFF00FF00L;
        long backgroundColor = 0xFF0000FFL;

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, themeColor);
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor);

        WebappInfo info = WebappInfo.create(intent);
        assertEquals(themeColor, info.themeColor());
        assertEquals(backgroundColor, info.backgroundColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testScopeIntentCreation() {
        String scope = "https://www.foo.com";
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_SCOPE, scope);
        WebappInfo info = WebappInfo.create(intent);
        assertEquals(scope, info.scopeUri().toString());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentScopeFallback() {
        String url = "https://www.foo.com/homepage.html";
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        WebappInfo info = WebappInfo.create(intent);
        assertEquals(ShortcutHelper.getScopeFromUrl(url), info.scopeUri().toString());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentDisplayMode() {
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MinimalUi);
        WebappInfo info = WebappInfo.create(intent);
        assertEquals(WebDisplayMode.MinimalUi, info.displayMode());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentOrientation() {
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.LANDSCAPE);
        WebappInfo info = WebappInfo.create(intent);
        assertEquals(ScreenOrientationValues.LANDSCAPE, info.orientation());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testIntentGeneratedIcon() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        // Default value.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);

            assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to true.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, true);

            assertTrue(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to false.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, false);

            assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to something else than a boolean.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, "true");

            assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }
    }

    /**
     * Creates intent with url and id. If the url or id are not set WebappInfo#create() returns
     * null.
     */
    private Intent createIntentWithUrlAndId() {
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, "web app id");
        intent.putExtra(ShortcutHelper.EXTRA_URL, "about:blank");
        return intent;
    }
}
