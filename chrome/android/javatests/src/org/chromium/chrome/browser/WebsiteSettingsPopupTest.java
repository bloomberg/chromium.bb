// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

/**
 * Tests for WebsiteSettingsPopup
 */
public class WebsiteSettingsPopupTest extends TestCase {
    @SmallTest
    public void testPrepareUrlForDisplay() {
        assertEquals("Encode suspicious message",
                WebsiteSettingsPopup.prepareUrlForDisplay(
                        "http://example.com/#  WARNING  \u00A0Chrome has detected malware on your"
                        + " device!"),
                "http://example.com/#%20%20WARNING%20%20%C2%A0Chrome%20has%20detected%20malware%20"
                        + "on%20your%20device!");
        assertEquals("Do not encode valid Unicode fragment",
                WebsiteSettingsPopup.prepareUrlForDisplay("http://example.com/#Düsseldorf"),
                "http://example.com/#Düsseldorf");
        assertEquals("Encode fragment with spaces",
                WebsiteSettingsPopup.prepareUrlForDisplay("http://example.com/#hi how are you"),
                "http://example.com/#hi%20how%20are%20you");
        assertEquals("Encode fragment with Unicode whitespace",
                WebsiteSettingsPopup.prepareUrlForDisplay("http://example.com/#em\u2003space"),
                "http://example.com/#em%E2%80%83space");
        assertEquals("Do not encode reserved URI characters or valid Unicode",
                WebsiteSettingsPopup.prepareUrlForDisplay("http://example.com/?q=a#Düsseldorf,"
                        + " Germany"),
                "http://example.com/?q=a#Düsseldorf,%20Germany");
        assertEquals("Preserve characters from supplementary Unicode planes",
                WebsiteSettingsPopup.prepareUrlForDisplay("http://example.com/#\uD835\uDC9Cstral"),
                "http://example.com/#\uD835\uDC9Cstral");
    }
}
