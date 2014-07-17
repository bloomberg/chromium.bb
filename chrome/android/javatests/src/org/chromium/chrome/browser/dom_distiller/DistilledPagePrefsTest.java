// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.test.suitebuilder.annotation.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.Theme;

/**
 * Test class for {@link DistilledPagePrefs}.
 */
public class DistilledPagePrefsTest extends ChromeShellTestBase {

    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetPrefs() throws InterruptedException {
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
        DomDistillerService service = DomDistillerServiceFactory.
            getForProfile(Profile.getLastUsedProfile());
        assertNotNull(service);
        DistilledPagePrefs distilledPagePrefs = service.getDistilledPagePrefs();
        assertNotNull(distilledPagePrefs);
        // Check default theme.
        assertEquals(distilledPagePrefs.getTheme(), Theme.LIGHT);
        // Check that theme can be correctly set.
        distilledPagePrefs.setTheme(Theme.DARK);
        assertEquals(Theme.DARK, distilledPagePrefs.getTheme());
        distilledPagePrefs.setTheme(Theme.LIGHT);
        assertEquals(Theme.LIGHT, distilledPagePrefs.getTheme());
        distilledPagePrefs.setTheme(Theme.SEPIA);
        assertEquals(Theme.SEPIA, distilledPagePrefs.getTheme());
    }
}
