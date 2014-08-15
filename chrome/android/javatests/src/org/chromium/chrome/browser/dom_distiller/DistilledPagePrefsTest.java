// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.test.suitebuilder.annotation.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.FontFamily;
import org.chromium.components.dom_distiller.core.Theme;
import org.chromium.content.browser.test.util.UiUtils;

/**
 * Test class for {@link DistilledPagePrefs}.
 */
public class DistilledPagePrefsTest extends ChromeShellTestBase {

    private DistilledPagePrefs mDistilledPagePrefs;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
        getDistilledPagePrefs();
    }

    private void getDistilledPagePrefs() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                DomDistillerService domDistillerService = DomDistillerServiceFactory.
                        getForProfile(Profile.getLastUsedProfile());
                mDistilledPagePrefs = domDistillerService.getDistilledPagePrefs();
            }
        });
    }

    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetTheme() throws InterruptedException {
        // Check the default theme.
        assertEquals(Theme.LIGHT, mDistilledPagePrefs.getTheme());
        // Check that theme can be correctly set.
        setTheme(Theme.DARK);
        assertEquals(Theme.DARK, mDistilledPagePrefs.getTheme());
        setTheme(Theme.LIGHT);
        assertEquals(Theme.LIGHT, mDistilledPagePrefs.getTheme());
        setTheme(Theme.SEPIA);
        assertEquals(Theme.SEPIA, mDistilledPagePrefs.getTheme());
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserverTheme() throws InterruptedException {
        TestingObserver testObserver = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserver);

        setTheme(Theme.DARK);
        // Assumes that callback does not occur immediately.
        assertNull(testObserver.getTheme());
        UiUtils.settleDownUI(getInstrumentation());
        // Check that testObserver's theme has been updated,
        assertEquals(Theme.DARK, testObserver.getTheme());
        mDistilledPagePrefs.removeObserver(testObserver);
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testMultipleObserversTheme() throws InterruptedException {
        TestingObserver testObserverOne = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserverOne);
        TestingObserver testObserverTwo = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserverTwo);

        setTheme(Theme.SEPIA);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals(Theme.SEPIA, testObserverOne.getTheme());
        assertEquals(Theme.SEPIA, testObserverTwo.getTheme());
        mDistilledPagePrefs.removeObserver(testObserverOne);

        setTheme(Theme.DARK);
        UiUtils.settleDownUI(getInstrumentation());
        // Check that testObserverOne's theme is not changed but testObserverTwo's is.
        assertEquals(Theme.SEPIA, testObserverOne.getTheme());
        assertEquals(Theme.DARK, testObserverTwo.getTheme());
        mDistilledPagePrefs.removeObserver(testObserverTwo);
    }

    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetFontFamily() throws InterruptedException {
        // Check the default font family.
        assertEquals(FontFamily.SANS_SERIF, mDistilledPagePrefs.getFontFamily());
        // Check that font family can be correctly set.
        setFontFamily(FontFamily.SERIF);
        assertEquals(FontFamily.SERIF, mDistilledPagePrefs.getFontFamily());
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserverFontFamily() throws InterruptedException {
        TestingObserver testObserver = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserver);

        setFontFamily(FontFamily.SERIF);
        // Assumes that callback does not occur immediately.
        assertNull(testObserver.getFontFamily());
        UiUtils.settleDownUI(getInstrumentation());
        // Check that testObserver's font family has been updated,
        assertEquals(FontFamily.SERIF, testObserver.getFontFamily());
        mDistilledPagePrefs.removeObserver(testObserver);
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testMultipleObserversFontFamily() throws InterruptedException {
        TestingObserver testObserverOne = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserverOne);
        TestingObserver testObserverTwo = new TestingObserver();
        mDistilledPagePrefs.addObserver(testObserverTwo);

        setFontFamily(FontFamily.MONOSPACE);
        UiUtils.settleDownUI(getInstrumentation());
        assertEquals(FontFamily.MONOSPACE, testObserverOne.getFontFamily());
        assertEquals(FontFamily.MONOSPACE, testObserverTwo.getFontFamily());
        mDistilledPagePrefs.removeObserver(testObserverOne);

        setFontFamily(FontFamily.SERIF);
        UiUtils.settleDownUI(getInstrumentation());
        // Check that testObserverOne's font family is not changed but testObserverTwo's is.
        assertEquals(FontFamily.MONOSPACE, testObserverOne.getFontFamily());
        assertEquals(FontFamily.SERIF, testObserverTwo.getFontFamily());
        mDistilledPagePrefs.removeObserver(testObserverTwo);
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testRepeatedAddAndDeleteObserver() throws InterruptedException {
        TestingObserver test = new TestingObserver();

        // Should successfully add the observer the first time.
        assertTrue(mDistilledPagePrefs.addObserver(test));
        // Observer cannot be added again, should return false.
        assertFalse(mDistilledPagePrefs.addObserver(test));

        // Delete the observer the first time.
        assertTrue(mDistilledPagePrefs.removeObserver(test));
        // Observer cannot be deleted again, should return false.
        assertFalse(mDistilledPagePrefs.removeObserver(test));
    }

    private static class TestingObserver implements DistilledPagePrefs.Observer {
        private FontFamily mFontFamily;
        private Theme mTheme;

        public TestingObserver() {}

        public FontFamily getFontFamily() {
            return mFontFamily;
        }

        public void onChangeFontFamily(FontFamily font) {
            mFontFamily = font;
        }

        public Theme getTheme() {
            return mTheme;
        }

        public void onChangeTheme(Theme theme) {
            mTheme = theme;
        }
    }

    private void setFontFamily(final FontFamily font) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                mDistilledPagePrefs.setFontFamily(font);
            }
        });
    }

    private void setTheme(final Theme theme) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                mDistilledPagePrefs.setTheme(theme);
            }
        });
    }
}
