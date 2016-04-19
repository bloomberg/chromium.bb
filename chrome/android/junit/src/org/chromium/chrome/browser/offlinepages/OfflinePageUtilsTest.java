// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.os.Environment;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.shadows.ShadowMultiDex;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.io.File;

/**
 * Unit tests for OfflinePageUtils.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        application = BaseChromiumApplication.class,
        shadows = { OfflinePageUtilsTest.WrappedEnvironment.class, ShadowMultiDex.class })
public class OfflinePageUtilsTest {

    @Mock private File mMockDataDirectory;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        WrappedEnvironment.setDataDirectoryForTest(mMockDataDirectory);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetFreeSpaceInBytes() {
        when(mMockDataDirectory.getUsableSpace()).thenReturn(1234L);
        assertEquals(1234L, OfflinePageUtils.getFreeSpaceInBytes());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetTotalSpaceInBytes() {
        when(mMockDataDirectory.getTotalSpace()).thenReturn(56789L);
        assertEquals(56789L, OfflinePageUtils.getTotalSpaceInBytes());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStripSchemeFromOnlineUrl() {
        // Only scheme gets stripped.
        assertEquals("cs.chromium.org",
                OfflinePageUtils.stripSchemeFromOnlineUrl("https://cs.chromium.org"));
        assertEquals("cs.chromium.org",
                OfflinePageUtils.stripSchemeFromOnlineUrl("http://cs.chromium.org"));
        // If there is no scheme, nothing changes.
        assertEquals("cs.chromium.org",
                OfflinePageUtils.stripSchemeFromOnlineUrl("cs.chromium.org"));
        // Path is not touched/changed.
        String urlWithPath = "code.google.com/p/chromium/codesearch#search"
                + "/&q=offlinepageutils&sq=package:chromium&type=cs";
        assertEquals(urlWithPath,
                OfflinePageUtils.stripSchemeFromOnlineUrl("https://" + urlWithPath));
        // Beginning and ending spaces get trimmed.
        assertEquals("cs.chromium.org",
                OfflinePageUtils.stripSchemeFromOnlineUrl("  https://cs.chromium.org  "));
    }

    /** A shadow/wrapper of android.os.Environment that allows injecting a test directory. */
    @Implements(Environment.class)
    public static class WrappedEnvironment {
        private static File sDataDirectory = null;

        public static void setDataDirectoryForTest(File testDirectory) {
            sDataDirectory = testDirectory;
        }

        @Implementation
        public static File getDataDirectory() {
            return sDataDirectory;
        }
    }
}
