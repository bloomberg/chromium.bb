// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Application Menu tests.
 */
public class MenuTest extends ChromeTabbedActivityTestBase {
    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>poit.</html>");

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Make sure the popup menu gets created, then add our observer to it.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity().getAppMenuHandler().isAppMenuShowing()) return true;
                pressKey(KeyEvent.KEYCODE_MENU);
                return false;
            }
        }));
    }

    /**
     * Verify opening a new tab from the menu.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testMenuNewTab() throws InterruptedException {
        final int tabCountBefore = getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        final int tabCountAfter = getActivity().getCurrentTabModel().getCount();
        assertTrue("Expected: " + (tabCountBefore + 1) + " Got: " + tabCountAfter,
                tabCountBefore + 1 == tabCountAfter);
    }

    private void pressKey(int keycode) {
        getInstrumentation().sendKeyDownUpSync(keycode);
        getInstrumentation().waitForIdleSync();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(TEST_URL);
    }
}
