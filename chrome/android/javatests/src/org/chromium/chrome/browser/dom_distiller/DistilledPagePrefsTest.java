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
    public void testGetAndSetPrefs() throws InterruptedException {
        // Check the default theme.
        assertEquals(mDistilledPagePrefs.getTheme(), Theme.LIGHT);
        // Check that theme can be correctly set.
        mDistilledPagePrefs.setTheme(Theme.DARK);
        assertEquals(Theme.DARK, mDistilledPagePrefs.getTheme());
        mDistilledPagePrefs.setTheme(Theme.LIGHT);
        assertEquals(Theme.LIGHT, mDistilledPagePrefs.getTheme());
        mDistilledPagePrefs.setTheme(Theme.SEPIA);
        assertEquals(Theme.SEPIA, mDistilledPagePrefs.getTheme());
    }

    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserver() throws InterruptedException {
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
    public void testMultipleObservers() throws InterruptedException {
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
        private Theme mTheme;

        public TestingObserver() {}

        public Theme getTheme() {
            return mTheme;
        }

        public void onChangeTheme(Theme theme) {
            mTheme = theme;
        }
    }

    private void setTheme(final Theme theme) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                mDistilledPagePrefs.setTheme(theme);
            }
        });
    }
}
