// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests starting the activity with URLs.
 */
@RetryOnFailure
public class MainActivityWithURLTest extends ChromeTabbedActivityTestBase {

    @Override
    public void startMainActivity() {
        // Don't launch activity automatically.
    }

    /**
     * Verify launch the activity with URL.
     */
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivityWithURL() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        try {
            // Launch chrome
            startMainActivityWithURL(testServer.getURL(
                    "/chrome/test/data/android/simple.html"));
            String expectedTitle = "Activity test page";
            TabModel model = getActivity().getCurrentTabModel();
            String title = model.getTabAt(model.index()).getTitle();
            assertEquals(expectedTitle, title);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Launch and verify URL is neither null nor empty.
     */
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivity() throws Exception {
        // Launch chrome
        startMainActivityFromLauncher();
        String currentUrl = getActivity().getActivityTab().getUrl();
        assertNotNull(currentUrl);
        assertEquals(false, currentUrl.isEmpty());
    }

    /**
     * Launch a NTP and make sure it loads correctly. This makes sure the
     * NTP loading complete notification is received.
     */
    @SmallTest
    @Feature({"Navigation"})
    public void testNewTabPageLaunch() throws Exception {
        // Launch chrome with NTP.
        startMainActivityWithURL(UrlConstants.NTP_URL);
        String currentUrl = getActivity().getActivityTab().getUrl();
        assertNotNull(currentUrl);
        assertEquals(false, currentUrl.isEmpty());

        // Open NTP.
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        currentUrl = getActivity().getActivityTab().getUrl();
        assertNotNull(currentUrl);
        assertEquals(false, currentUrl.isEmpty());
    }
}
