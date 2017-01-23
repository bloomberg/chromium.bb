// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;

/**
 * Tests of {@link DownloadUtils}.
 */
public class DownloadUtilsTest extends InstrumentationTestCase {
    /**
     * Test {@link DownloadUtils#getAbbrieviatedFileName()} method.
     */
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbrieviatedFileName() {
        assertEquals("123.pdf", DownloadUtils.getAbbreviatedFileName("123.pdf", 10));
        assertEquals("1" + DownloadUtils.ELLIPSIS,
                DownloadUtils.getAbbreviatedFileName("123.pdf", 1));
        assertEquals("12" + DownloadUtils.ELLIPSIS,
                DownloadUtils.getAbbreviatedFileName("1234567", 2));
        assertEquals("123" + DownloadUtils.ELLIPSIS + ".pdf",
                DownloadUtils.getAbbreviatedFileName("1234567.pdf", 7));
    }
}
