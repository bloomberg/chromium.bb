// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.concurrent.Callable;

/**
 * Tests for {@link FontSizePrefs}.
 */
public class FontSizePrefsTest extends NativeLibraryTestBase {

    private static final float EPSILON = 0.001f;
    private FontSizePrefs mFontSizePrefs;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        resetSharedPrefs();
        Context context = getInstrumentation().getTargetContext();
        mFontSizePrefs = getFontSizePrefs(context);
        setSystemFontScaleForTest(1.0f);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        resetSharedPrefs();
    }

    private void resetSharedPrefs() {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(FontSizePrefs.PREF_USER_SET_FORCE_ENABLE_ZOOM);
        editor.remove(FontSizePrefs.PREF_USER_FONT_SCALE_FACTOR);
        editor.apply();
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testForceEnableZoom() {
        assertEquals(false, getForceEnableZoom());

        TestingObserver observer = createAndAddFontSizePrefsObserver();

        setUserFontScaleFactor(1.5f);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();

        setForceEnableZoomFromUser(true);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(1.5f);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();

        setForceEnableZoomFromUser(false);
        assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(1.5f);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();

        // Force enable zoom should depend on fontScaleFactor, not on userFontScaleFactor.
        setSystemFontScaleForTest(2.0f);
        assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setSystemFontScaleForTest(1.0f);
        assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testFontScaleFactor() {
        assertEquals(1f, getUserFontScaleFactor(), EPSILON);
        assertEquals(1f, getFontScaleFactor(), EPSILON);

        TestingObserver observer = createAndAddFontSizePrefsObserver();

        setUserFontScaleFactor(1.5f);
        assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        assertEquals(1.5f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setUserFontScaleFactor(0.7f);
        assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        // Force enable zoom shouldn't affect font scale factor.
        setForceEnableZoomFromUser(true);
        assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setForceEnableZoomFromUser(false);
        assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        // System font scale should affect fontScaleFactor, but not userFontScaleFactor.
        setSystemFontScaleForTest(1.3f);
        assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        assertEquals(0.7f * 1.3f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setUserFontScaleFactor(1.5f);
        assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        assertEquals(1.5f * 1.3f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setSystemFontScaleForTest(0.8f);
        assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        assertEquals(1.5f * 0.8f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testUpgradeToUserFontScaleFactor() {
        setSystemFontScaleForTest(1.3f);
        setUserFontScaleFactor(1.5f);

        // Delete PREF_USER_FONT_SCALE_FACTOR. This simulates the condition just after upgrading to
        // M51, when userFontScaleFactor was added.
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(FontSizePrefs.PREF_USER_FONT_SCALE_FACTOR);
        editor.commit();

        // Intial userFontScaleFactor should be set to fontScaleFactor / systemFontScale.
        assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        assertEquals(1.5f * 1.3f, getFontScaleFactor(), EPSILON);
    }

    private class TestingObserver implements FontSizePrefsObserver {
        private float mUserFontScaleFactor = getUserFontScaleFactor();
        private float mFontScaleFactor = getFontScaleFactor();
        private boolean mForceEnableZoom = getForceEnableZoom();

        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            mFontScaleFactor = fontScaleFactor;
            mUserFontScaleFactor = userFontScaleFactor;
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            mForceEnableZoom = enabled;
        }

        private void assertConsistent() {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertEquals(getUserFontScaleFactor(), mUserFontScaleFactor, EPSILON);
                    assertEquals(getFontScaleFactor(), mFontScaleFactor, EPSILON);
                    assertEquals(getForceEnableZoom(), mForceEnableZoom);
                }
            });
        }
    }

    private FontSizePrefs getFontSizePrefs(final Context context) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<FontSizePrefs>() {
            @Override
            public FontSizePrefs call() {
                return FontSizePrefs.getInstance(context);
            }
        });
    }

    private TestingObserver createAndAddFontSizePrefsObserver() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<TestingObserver>() {
            @Override
            public TestingObserver call() {
                TestingObserver observer = new TestingObserver();
                mFontSizePrefs.addObserver(observer);
                return observer;
            }
        });
    }

    private void setUserFontScaleFactor(final float fontsize) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setUserFontScaleFactor(fontsize);
            }
        });
    }

    private float getUserFontScaleFactor() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Float>() {
            @Override
            public Float call() {
                return mFontSizePrefs.getUserFontScaleFactor();
            }
        });
    }

    private float getFontScaleFactor() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Float>() {
            @Override
            public Float call() {
                return mFontSizePrefs.getFontScaleFactor();
            }
        });
    }

    private void setForceEnableZoomFromUser(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setForceEnableZoomFromUser(enabled);
            }
        });
    }

    private boolean getForceEnableZoom() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mFontSizePrefs.getForceEnableZoom();
            }
        });
    }

    private void setSystemFontScaleForTest(final float systemFontScale) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setSystemFontScaleForTest(systemFontScale);
                mFontSizePrefs.onSystemFontScaleChanged();
            }
        });
    }
}
