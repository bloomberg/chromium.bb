// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.ArrayList;

/**
 * Tests whether popup windows appear.
 * In document mode, this will end up spawning multiple Activities.
 */
public class PopupTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String POPUP_HTML_FILENAME =
            TestHttpServerClient.getUrl("chrome/test/data/android/popup_test.html");

    public PopupTest() {
        super(ChromeActivity.class);
    }

    private int getNumInfobarsShowing() {
        return getActivity().getActivityTab().getInfoBarContainer().getInfoBars().size();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(getNumInfobarsShowing() == 0);
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    @Feature({"Popup"})
    public void testPopupInfobarAppears() throws Exception {
        loadUrl(POPUP_HTML_FILENAME);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        }));
    }

    @MediumTest
    @Feature({"Popup"})
    public void testPopupWindowsAppearWhenAllowed() throws Exception {
        boolean isDocumentMode =
                FeatureUtilities.isDocumentMode(ApplicationStatus.getApplicationContext());
        final TabModelSelector selector = isDocumentMode
                ? ChromeApplication.getDocumentTabModelSelector()
                : getActivity().getTabModelSelector();

        loadUrl(POPUP_HTML_FILENAME);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        }));
        assertEquals(1, selector.getTotalTabCount());
        ArrayList<InfoBar> infobars = selector.getCurrentTab().getInfoBarContainer().getInfoBars();
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

        // Document mode popups appear slowly and sequentially to prevent Android from throwing them
        // away, so use a long timeout.  http://crbug.com/498920.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() != 0) return false;
                return TextUtils.equals("Popup #3", selector.getCurrentTab().getTitle());
            }
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL));

        assertEquals(4, selector.getTotalTabCount());
        int currentTabId = selector.getCurrentTab().getId();

        // Test that revisiting the original page makes popup windows immediately.
        loadUrl(POPUP_HTML_FILENAME);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() != 0) return false;
                if (selector.getTotalTabCount() != 7) return false;
                return TextUtils.equals("Popup #3", selector.getCurrentTab().getTitle());
            }
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL));
        assertNotSame(currentTabId, selector.getCurrentTab().getId());
    }
}
