// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.UiUtils;

import java.util.concurrent.Callable;

/**
 * Test class for {@link FontSizePrefs}.
 */
public class FontSizePrefsTest extends ChromeShellTestBase {

    private FontSizePrefs mFontSizePrefs;
    private SharedPreferences.Editor mSharedPreferencesEditor;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext();
        startChromeBrowserProcessSync(context);
        getFontSizePrefs(context);
        mSharedPreferencesEditor = PreferenceManager.getDefaultSharedPreferences(context).edit();
    }

    @Override
    protected void tearDown() throws Exception {
        mSharedPreferencesEditor.remove(FontSizePrefs.PREF_USER_SET_FORCE_ENABLE_ZOOM).commit();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testGetAndSetFontAndForceEnableZoom() throws InterruptedException {
        // Check default font.
        assertEquals(1f, getFontScale());
        // Check that setting the value of font scale factor works.
        float newTextScale = 1.2f;
        setFontScale(newTextScale);
        assertEquals(newTextScale, getFontScale());

        // Check the default value of force enable zoom.
        assertFalse(getForceEnableZoom());
        // Check that setting the value of force enable zoom works.
        setForceEnableZoom(true);
        assertTrue(getForceEnableZoom());
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testGetAndSetUserSetForceEnableZoom() throws InterruptedException {
        // Check the default value of user set force enable zoom.
        assertFalse(mFontSizePrefs.getUserSetForceEnableZoom());
        // Check that setting the value of user set force enable zoom works.
        setUserSetForceEnableZoom(true);
        assertTrue(mFontSizePrefs.getUserSetForceEnableZoom());
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testObserversForceEnableZoom() throws InterruptedException {
        TestingObserver test1 = new TestingObserver();
        TestingObserver test2 = new TestingObserver();
        mFontSizePrefs.addObserver(test1);
        mFontSizePrefs.addObserver(test2);

        // Checks that force enable zoom for both observers is correctly changed.
        setForceEnableZoom(true);
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue(test1.getForceEnableZoom());
        assertTrue(test2.getForceEnableZoom());

        // Checks that removing observer and setting force enable zoom works.
        mFontSizePrefs.removeObserver(test1);
        setForceEnableZoom(false);
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue(test1.getForceEnableZoom());
        assertFalse(test2.getForceEnableZoom());
        mFontSizePrefs.removeObserver(test2);
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testObserversFontScale() throws InterruptedException {
        TestingObserver test1 = new TestingObserver();
        TestingObserver test2 = new TestingObserver();
        mFontSizePrefs.addObserver(test1);
        mFontSizePrefs.addObserver(test2);

        // Checks that font scale for both observers is correctly changed.
        float newTextScale = 1.2f;
        setFontScale(newTextScale);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals(newTextScale, test1.getFontScaleFactor());
        assertEquals(newTextScale, test2.getFontScaleFactor());

        // Checks that removing observer and setting font works.
        float newerTextScale = 1.4f;
        mFontSizePrefs.removeObserver(test1);
        setFontScale(newerTextScale);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals(newTextScale, test1.getFontScaleFactor());
        assertEquals(newerTextScale, test2.getFontScaleFactor());
        mFontSizePrefs.removeObserver(test2);
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testObserversUserSetForceEnableZoom() throws InterruptedException {
        TestingObserver test1 = new TestingObserver();
        TestingObserver test2 = new TestingObserver();
        mFontSizePrefs.addObserver(test1);
        mFontSizePrefs.addObserver(test2);

        // Checks that force enable zoom for both observers is correctly changed.
        setUserSetForceEnableZoom(true);
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue(test1.getUserSetForceEnableZoom());
        assertTrue(test2.getUserSetForceEnableZoom());

        // Checks that removing observer and setting force enable zoom works.
        mFontSizePrefs.removeObserver(test1);
        setUserSetForceEnableZoom(false);
        UiUtils.settleDownUI(getInstrumentation());
        assertTrue(test1.getUserSetForceEnableZoom());
        assertFalse(test2.getUserSetForceEnableZoom());
        mFontSizePrefs.removeObserver(test2);
    }

    @SmallTest
    @Feature({"Accessibility"})
    public void testMultipleAddMultipleDeleteObservers() throws InterruptedException {
        TestingObserver test = new TestingObserver();

        // Should successfully add the observer the first time.
        assertTrue(mFontSizePrefs.addObserver(test));
        // Observer cannot be added again, should return false.
        assertFalse(mFontSizePrefs.addObserver(test));

        // Delete the observer the first time.
        assertTrue(mFontSizePrefs.removeObserver(test));
        // Observer cannot be deleted again, should return false.
        assertFalse(mFontSizePrefs.removeObserver(test));
    }

    private static class TestingObserver implements FontSizePrefs.Observer {
        private float mFontSize;
        private boolean mForceEnableZoom;
        private boolean mUserSetForceEnableZoom;

        public TestingObserver() {
            mFontSize = 1;
            mForceEnableZoom = false;
            mUserSetForceEnableZoom = false;
        }

        public float getFontScaleFactor() {
            return mFontSize;
        }

        @Override
        public void onChangeFontSize(float font) {
            mFontSize = font;
        }

        public boolean getForceEnableZoom() {
            return mForceEnableZoom;
        }

        @Override
        public void onChangeForceEnableZoom(boolean enabled) {
            mForceEnableZoom = enabled;
        }

        @Override
        public void onChangeUserSetForceEnableZoom(boolean enabled) {
            mUserSetForceEnableZoom = enabled;
        }

        public boolean getUserSetForceEnableZoom() {
            return mUserSetForceEnableZoom;
        }
    }

    private void getFontSizePrefs(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs = FontSizePrefs.getInstance(context);
            }
        });
    }

    private void setFontScale(final float fontsize) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setFontScaleFactor(fontsize);
            }
        });
    }

    private float getFontScale() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Float>() {
            @Override
            public Float call() {
                return mFontSizePrefs.getFontScaleFactor();
            }
        });
    }

    private void setForceEnableZoom(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setForceEnableZoom(enabled);
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

    private void setUserSetForceEnableZoom(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFontSizePrefs.setUserSetForceEnableZoom(enabled);
            }
        });
    }
}
