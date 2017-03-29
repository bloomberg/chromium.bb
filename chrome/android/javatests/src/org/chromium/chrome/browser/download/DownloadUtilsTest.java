// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests of {@link DownloadUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadUtilsTest {
    /**
     * Test {@link DownloadUtils#getAbbrieviatedFileName()} method.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbrieviatedFileName() {
        Assert.assertEquals("123.pdf", DownloadUtils.getAbbreviatedFileName("123.pdf", 10));
        Assert.assertEquals(
                "1" + DownloadUtils.ELLIPSIS, DownloadUtils.getAbbreviatedFileName("123.pdf", 1));
        Assert.assertEquals(
                "12" + DownloadUtils.ELLIPSIS, DownloadUtils.getAbbreviatedFileName("1234567", 2));
        Assert.assertEquals("123" + DownloadUtils.ELLIPSIS + ".pdf",
                DownloadUtils.getAbbreviatedFileName("1234567.pdf", 7));
    }
}
