// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for WebsiteAddress.
 * loadNativeLibraryAndInitBrowserProcess seems to be flaky on ICS.
 * http://crbug.com/431717
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.JELLY_BEAN)
public class WebsiteAddressTest extends NativeLibraryTestBase {

    @Smoke
    @SmallTest
    @Feature({"Preferences", "Main"})
    public void testCreate() {
        assertEquals(null, WebsiteAddress.create(null));
        assertEquals(null, WebsiteAddress.create(""));

        WebsiteAddress httpAddress = WebsiteAddress.create("http://a.google.com");
        assertEquals("http://a.google.com", httpAddress.getOrigin());
        assertEquals("a.google.com", httpAddress.getHost());
        assertEquals("a.google.com", httpAddress.getTitle());

        WebsiteAddress http8080Address = WebsiteAddress.create("http://a.google.com:8080/");
        assertEquals("http://a.google.com:8080", http8080Address.getOrigin());
        assertEquals("a.google.com", http8080Address.getHost());
        assertEquals("http://a.google.com:8080", http8080Address.getTitle());

        WebsiteAddress httpsAddress = WebsiteAddress.create("https://a.google.com/");
        assertEquals("https://a.google.com", httpsAddress.getOrigin());
        assertEquals("a.google.com", httpsAddress.getHost());
        assertEquals("https://a.google.com", httpsAddress.getTitle());

        WebsiteAddress hostAddress = WebsiteAddress.create("a.google.com");
        assertEquals(null, hostAddress.getOrigin());
        assertEquals("a.google.com", hostAddress.getHost());
        assertEquals("a.google.com", hostAddress.getTitle());

        WebsiteAddress anySubdomainAddress = WebsiteAddress.create("[*.]google.com");
        assertEquals(null, anySubdomainAddress.getOrigin());
        assertEquals("google.com", anySubdomainAddress.getHost());
        assertEquals("google.com", anySubdomainAddress.getTitle());
    }

    @SmallTest
    @Feature({"Preferences"})
    public void testEqualsHashCodeCompareTo() {
        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();

        Object[][] testData = {
            { 0, "http://google.com", "http://google.com" },
            { -1, "[*.]google.com", "http://google.com" },
            { -1, "[*.]google.com", "http://a.google.com" },
            { -1, "[*.]a.com", "[*.]b.com" },
            { 0, "[*.]google.com", "google.com" },
            { -1, "[*.]google.com", "a.google.com" },
            { -1, "http://google.com", "http://a.google.com" },
            { -1, "http://a.google.com", "http://a.a.google.com" },
            { -1, "http://a.a.google.com", "http://a.b.google.com" },
            { 1, "http://a.b.google.com", "http://google.com" },
            { -1, "http://google.com", "https://google.com" },
            { -1, "http://google.com", "https://a.google.com" },
            { 1, "https://b.google.com", "https://a.google.com" },
            { -1, "http://a.com", "http://b.com" },
            { -1, "http://a.com", "http://a.b.com" }
        };

        for (int i = 0; i < testData.length; ++i) {
            Object[] testRow = testData[i];

            int compareToResult = (Integer) testRow[0];

            String string1 = (String) testRow[1];
            String string2 = (String) testRow[2];

            WebsiteAddress addr1 = WebsiteAddress.create(string1);
            WebsiteAddress addr2 = WebsiteAddress.create(string2);

            assertEquals(
                    "\"" + string1 + "\" vs \"" + string2 + "\"",
                    compareToResult,
                    Integer.signum(addr1.compareTo(addr2)));

            // Test that swapping arguments gives an opposite result.
            assertEquals(
                    "\"" + string2 + "\" vs \"" + string1 + "\"",
                    -compareToResult,
                    Integer.signum(addr2.compareTo(addr1)));

            if (compareToResult == 0) {
                assertTrue(addr1.equals(addr2));
                assertTrue(addr2.equals(addr1));
                assertEquals(addr1.hashCode(), addr2.hashCode());
            } else {
                assertFalse(addr1.equals(addr2));
                assertFalse(addr2.equals(addr1));
                // Note: hash codes could still be the same.
            }
        }
    }
}
