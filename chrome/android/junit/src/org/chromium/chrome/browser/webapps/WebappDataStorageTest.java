// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Tests the WebappDataStorage class by ensuring that it persists data to
 * SharedPreferences as expected.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappDataStorageTest {

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    @Before
    public void setUp() throws Exception {
        mSharedPreferences = Robolectric.application
                .getSharedPreferences("webapp_test", Context.MODE_PRIVATE);
        mCallbackCalled = false;
    }

    @Test
    @Feature("{Webapp}")
    public void testBackwardCompat() {
        assertEquals("webapp_", WebappDataStorage.SHARED_PREFS_FILE_PREFIX);
        assertEquals("splash_icon", WebappDataStorage.KEY_SPLASH_ICON);
        assertEquals("last_used", WebappDataStorage.KEY_LAST_USED);
    }

    @Test
    @Feature("{Webapp}")
    public void testLastUsedRetrieval() throws InterruptedException {
        mSharedPreferences.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, 100L)
                .commit();

        WebappDataStorage.getLastUsedTime(Robolectric.application, "test",
                new WebappDataStorage.FetchCallback<Long>() {
                    @Override
                    public void onDataRetrieved(Long readObject) {
                        mCallbackCalled = true;
                        assertEquals(100L, (long) readObject);
                    }
                });
        Robolectric.runBackgroundTasks();
        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature("{Webapp}")
    public void testOpenUpdatesLastUsed() throws InterruptedException {
        long before = System.currentTimeMillis();

        WebappDataStorage.open(Robolectric.application, "test");
        Robolectric.runBackgroundTasks();

        long after = System.currentTimeMillis();
        long value = mSharedPreferences.getLong(WebappDataStorage.KEY_LAST_USED, -1L);
        assertTrue("Timestamp is out of range", before <= value && value <= after);
    }

    @Test
    @Feature("{Webapp}")
    public void testSplashImageRetrieval() throws InterruptedException {
        final Bitmap expected = createBitmap();
        mSharedPreferences.edit()
                .putString(WebappDataStorage.KEY_SPLASH_ICON,
                        ShortcutHelper.encodeBitmapAsString(expected))
                .commit();
        WebappDataStorage.open(Robolectric.application, "test")
                .getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
                    @Override
                    public void onDataRetrieved(Bitmap actual) {
                        mCallbackCalled = true;

                        // TODO(lalitm) - once the Robolectric bug is fixed change to
                        // assertTrue(expected.sameAs(actual)).
                        // See bitmapEquals(Bitmap, Bitmap) for more information.
                        assertTrue(bitmapEquals(expected, actual));
                    }
                });
        Robolectric.runBackgroundTasks();
        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature("{Webapp}")
    public void testSplashImageUpdate() throws InterruptedException {
        final Bitmap expectedImage = createBitmap();
        WebappDataStorage.open(Robolectric.application, "test")
                .updateSplashScreenImage(expectedImage);
        Robolectric.runBackgroundTasks();
        assertEquals(ShortcutHelper.encodeBitmapAsString(expectedImage),
                mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_ICON, null));
    }

    // TODO(lalitm) - There seems to be a bug in Robolectric where a Bitmap
    // produced from a byte stream is hardcoded to be a 100x100 bitmap with
    // ARGB_8888 pixel format. Because of this, we need to work around the
    // equality check of bitmaps. Remove this once the bug is fixed.
    private static boolean bitmapEquals(Bitmap expected, Bitmap actual) {
        if (actual.getWidth() != 100) return false;
        if (actual.getHeight() != 100) return false;
        if (!actual.getConfig().equals(Bitmap.Config.ARGB_8888)) return false;

        for (int i = 0; i < actual.getWidth(); i++) {
            for (int j = 0; j < actual.getHeight(); j++) {
                if (actual.getPixel(i, j) != 0) return false;
            }
        }
        return true;
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_4444);
    }
}