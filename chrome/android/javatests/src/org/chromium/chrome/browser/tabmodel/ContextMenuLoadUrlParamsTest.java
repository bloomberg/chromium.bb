// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.test.FlakyTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabWindowManager.TabModelSelectorFactory;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeoutException;

/**
 * Verifies URL load parameters set when triggering navigations from the context menu.
 */
public class ContextMenuLoadUrlParamsTest extends ChromeTabbedActivityTestBase {
    // Load parameters of the last call to openNewTab().
    LoadUrlParams mOpenNewTabLoadUrlParams;

    // Records parameters of calls to TabModelSelector methods and otherwise behaves like
    // TabModelSelectorImpl.
    class RecordingTabModelSelector extends TabModelSelectorImpl {
        @Override
        public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                boolean incognito) {
            mOpenNewTabLoadUrlParams = loadUrlParams;
            return super.openNewTab(loadUrlParams, type, parent, incognito);
        }

        public RecordingTabModelSelector(ChromeActivity activity, int selectorIndex,
                WindowAndroid windowAndroid) {
            super(activity, selectorIndex, windowAndroid);
        }
    }

    @Override
    public void setUp() throws Exception {
        // Plant RecordingTabModelSelector as the TabModelSelector used in Main. The factory has to
        // be set before super.setUp(), as super.setUp() creates Main and consequently the
        // TabModelSelector.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabWindowManager.getInstance().setTabModelSelectorFactory(
                        new TabModelSelectorFactory() {
                            @Override
                            public TabModelSelector buildSelector(ChromeActivity activity,
                                    WindowAndroid windowAndroid, int selectorIndex) {
                                return new RecordingTabModelSelector(activity, selectorIndex,
                                        windowAndroid);
                            }
                        });
            }
        });
        super.setUp();
    }

    /**
     * Verifies that the referrer is correctly set for "Open in new tab".
     * Bug: crbug.com/413216
     * @MediumTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testOpenInNewTabReferrer()
            throws InterruptedException, TimeoutException {
        String url =
                TestHttpServerClient.getUrl("chrome/test/data/android/context_menu_test.html");
        String expectedReferrer = url;
        triggerContextMenuLoad(url, "testLink", R.id.contextmenu_open_in_new_tab);

        assertNotNull(mOpenNewTabLoadUrlParams);
        assertEquals(expectedReferrer, mOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    /**
     * Verifies that the referrer is not set for "Open in new incognito tab".
     * Bug: crbug.com/413216
     * @MediumTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testOpenInIncognitoTabNoReferrer()
            throws InterruptedException, TimeoutException {
        String url =
                TestHttpServerClient.getUrl("chrome/test/data/android/context_menu_test.html");
        triggerContextMenuLoad(url, "testLink", R.id.contextmenu_open_in_incognito_tab);

        assertNotNull(mOpenNewTabLoadUrlParams);
        assertNull(mOpenNewTabLoadUrlParams.getReferrer());
    }

    /**
     * Verifies that the referrer is stripped from username and password fields.
     * Bug: crbug.com/413216
     * @MediumTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testOpenInNewTabSanitizeReferrer()
            throws InterruptedException, TimeoutException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/context_menu_test.html",
                "user", "pass");
        String expectedReferrer =
                TestHttpServerClient.getUrl("chrome/test/data/android/context_menu_test.html");
        assertTrue(url.contains("pass"));  // Sanity check.
        triggerContextMenuLoad(url, "testLink", R.id.contextmenu_open_in_new_tab);

        assertNotNull(mOpenNewTabLoadUrlParams);
        assertEquals(expectedReferrer, mOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    private void triggerContextMenuLoad(String url, String openerDomId, int menuItemId)
            throws InterruptedException, TimeoutException {
        loadUrl(url);
        assertWaitForPageScaleFactorMatch(0.5f);
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, openerDomId, menuItemId);
        getInstrumentation().waitForIdleSync();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

