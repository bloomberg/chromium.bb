// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.ArrayList;

/**
 * Tests whether popup windows appear.
 */
public class PopupTest extends ChromeShellTestBase {
    private static final String POPUP_HTML_FILENAME =
            TestHttpServerClient.getUrl("chrome/test/data/android/popup_test.html");

    private int getNumInfobarsShowing() {
        return getActivity().getActiveTab().getInfoBarContainer().getInfoBars().size();
    }

    private void loadPageCompletely(Tab tab, String url) throws Exception {
        TabLoadObserver observer = new TabLoadObserver(tab, url);
        assertTrue(CriteriaHelper.pollForCriteria(observer));
        waitForActiveShellToBeDoneLoading();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchChromeShellWithBlankPage();
        waitForActiveShellToBeDoneLoading();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(getNumInfobarsShowing() == 0);
            }
        });
    }

    public void testPopupInfobarAppears() throws Exception {
        loadPageCompletely(getActivity().getActiveTab(), POPUP_HTML_FILENAME);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        }));
    }

    public void testPopupWindowsAppearWhenAllowed() throws Exception {
        loadPageCompletely(getActivity().getActiveTab(), POPUP_HTML_FILENAME);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        }));
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        ArrayList<InfoBar> infobars =
                getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        assertEquals(1, infobars.size());

        // Wait until the animations are done, then click the "open popups" button.
        final InfoBar infobar = infobars.get(0);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return infobar.areControlsEnabled();
            }
        }));
        TouchCommon.singleClickView(infobar.getContentWrapper().findViewById(R.id.button_primary));

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() != 0) return false;
                if (getActivity().getTabModelSelector().getTotalTabCount() != 3) return false;
                if (!TextUtils.equals("Popup #2", getActivity().getActiveTab().getTitle())) {
                    return false;
                }

                return true;
            }
        }));
    }
}