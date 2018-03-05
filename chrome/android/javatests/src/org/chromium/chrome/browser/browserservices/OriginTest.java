// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests for org.chromium.net.Origin.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class OriginTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
    }

    /**
     * The actual conversion from a free form URL to an Origin is done in native and that is
     * thoroughly tested there. Here we only check that the transformation is performed, not that
     * it is complete and correct.
     */
    @Test
    @SmallTest
    public void testConstruction() {
        Origin origin = new Origin("http://www.example.com/path/to/page.html");
        Assert.assertEquals("http://www.example.com/", origin.toString());
    }

    @Test
    @SmallTest
    public void testEquality() {
        Origin origin1 = new Origin("http://www.example.com/page1.html");
        Origin origin2 = new Origin("http://www.example.com/page2.html");
        Assert.assertEquals(origin1, origin2);
        Assert.assertEquals(origin1.hashCode(), origin2.hashCode());

        // Note http*s*.
        Origin origin3 = new Origin("https://www.example.com/page3.html");
        Assert.assertNotEquals(origin1, origin3);
    }

    @Test
    @SmallTest
    public void testToUri() {
        Origin origin = new Origin(Uri.parse("http://www.example.com/page.html"));
        Uri uri = Uri.parse("http://www.example.com/");
        Assert.assertEquals(uri, origin.uri());
    }

    @Test
    @SmallTest
    public void testToString() {
        Origin origin = new Origin("http://www.example.com/page.html");
        Assert.assertEquals("http://www.example.com/", origin.toString());
    }
}
