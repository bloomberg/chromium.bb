// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;

/**
 * Tests whether popup windows appear.
 * In document mode, this will end up spawning multiple Activities.
 */
public class PopupTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String POPUP_HTML_PATH = "/chrome/test/data/android/popup_test.html";

    private String mPopupHtmlUrl;
    private EmbeddedTestServer mTestServer;

    public PopupTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    private int getNumInfobarsShowing() {
        return getInfoBars().size();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(getNumInfobarsShowing() == 0);
            }
        });

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        mPopupHtmlUrl = mTestServer.getURL(POPUP_HTML_PATH);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    @Feature({"Popup"})
    public void testPopupInfobarAppears() throws Exception {
        loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        });
    }

    @MediumTest
    @Feature({"Popup"})
    public void testPopupWindowsAppearWhenAllowed() throws Exception {
        boolean isDocumentMode =
                FeatureUtilities.isDocumentMode(ApplicationStatus.getApplicationContext());
        final TabModelSelector selector = isDocumentMode
                ? ChromeApplication.getDocumentTabModelSelector()
                : getActivity().getTabModelSelector();

        loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumInfobarsShowing() == 1;
            }
        });
        assertEquals(1, selector.getTotalTabCount());
        final InfoBarContainer container = selector.getCurrentTab().getInfoBarContainer();
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        assertEquals(1, infobars.size());

        // Wait until the animations are done, then click the "open popups" button.
        final InfoBar infobar = infobars.get(0);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !container.isAnimating();
            }
        });
        TouchCommon.singleClickView(infobar.getView().findViewById(R.id.button_primary));

        // Document mode popups appear slowly and sequentially to prevent Android from throwing them
        // away, so use a long timeout.  http://crbug.com/498920.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() != 0) return false;
                return TextUtils.equals("Popup #3", selector.getCurrentTab().getTitle());
            }
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertEquals(4, selector.getTotalTabCount());
        int currentTabId = selector.getCurrentTab().getId();

        // Test that revisiting the original page makes popup windows immediately.
        loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() != 0) return false;
                if (selector.getTotalTabCount() != 7) return false;
                return TextUtils.equals("Popup #3", selector.getCurrentTab().getTitle());
            }
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        assertNotSame(currentTabId, selector.getCurrentTab().getId());
    }
}
