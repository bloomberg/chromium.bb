// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabWindowManager.TabModelSelectorFactory;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeoutException;
import java.util.regex.Pattern;

/**
 * Verifies URL load parameters set when triggering navigations from the context menu.
 */
public class ContextMenuLoadUrlParamsTest extends ChromeTabbedActivityTestBase {
    private static final String HTML_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final Pattern SCHEME_SEPARATOR_RE = Pattern.compile("://");

    // Load parameters of the last call to openNewTab().
    LoadUrlParams mOpenNewTabLoadUrlParams;

    private EmbeddedTestServer mTestServer;

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

    public ContextMenuLoadUrlParamsTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    protected void setUp() throws Exception {
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

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Verifies that the referrer is correctly set for "Open in new tab".
     */
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabReferrer()
            throws InterruptedException, TimeoutException {
        triggerContextMenuLoad(mTestServer.getURL(HTML_PATH), "testLink",
                R.id.contextmenu_open_in_new_tab);

        assertNotNull(mOpenNewTabLoadUrlParams);
        assertEquals(mTestServer.getURL(HTML_PATH),
                mOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    /**
     * Verifies that the referrer is not set for "Open in new incognito tab".
     */
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInIncognitoTabNoReferrer()
            throws InterruptedException, TimeoutException {
        triggerContextMenuLoad(mTestServer.getURL(HTML_PATH), "testLink",
                R.id.contextmenu_open_in_incognito_tab);

        assertNotNull(mOpenNewTabLoadUrlParams);
        assertNull(mOpenNewTabLoadUrlParams.getReferrer());
    }

    /**
     * Verifies that the referrer is stripped from username and password fields.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabSanitizeReferrer()
            throws InterruptedException, TimeoutException {
        String testUrl = mTestServer.getURL(HTML_PATH);
        String[] schemeAndUrl = SCHEME_SEPARATOR_RE.split(testUrl, 2);
        assertEquals(2, schemeAndUrl.length);
        String testUrlUserPass = schemeAndUrl[0] + "://user:pass@" + schemeAndUrl[1];
        triggerContextMenuLoad(testUrlUserPass, "testLink", R.id.contextmenu_open_in_new_tab);
        assertNotNull(mOpenNewTabLoadUrlParams);
        assertEquals(testUrl, mOpenNewTabLoadUrlParams.getReferrer().getUrl());
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
