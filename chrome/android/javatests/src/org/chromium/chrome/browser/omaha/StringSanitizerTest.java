// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;

/** Tests the Omaha StringSanitizer. */
public class StringSanitizerTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Omaha"})
    public void testSanitizeStrings() {
        assertEquals("normal string", StringSanitizer.sanitize("Normal string"));
        assertEquals("extra spaces string",
                StringSanitizer.sanitize("\nExtra,  spaces;  string "));
        assertEquals("a quick brown fox jumped over the lazy dog",
                StringSanitizer.sanitize("  a\"quick;  brown,fox'jumped;over \nthe\rlazy\tdog\n"));
    }
}
