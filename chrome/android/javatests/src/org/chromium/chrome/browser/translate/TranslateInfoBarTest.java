// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate infobar, assumes it runs on a system with language
 * preferences set to English.
 *
 * Note: these tests all currently fail because they depend on a newer version of Google Play
 * Services than is installed on the test devices. See http://crbug.com/514449
 */
public class TranslateInfoBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NEVER_TRANSLATE_MESSAGE =
            "Would you like Google Chrome to offer to translate French pages from this"
                    + " site next time?";

    private InfoBarContainer mInfoBarContainer;
    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    public TranslateInfoBarTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mInfoBarContainer = getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        mInfoBarContainer.setAnimationListener(mListener);
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Test the translate language panel.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTranslateLanguagePanel() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        InfoBar infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBar));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBar));
        TranslateUtil.openLanguagePanel(this, infoBar);
    }

    /**
     * Test the "never translate" panel.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTranslateNeverPanel() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        InfoBar infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);

        assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        mListener.removeInfoBarAnimationFinished("Infobar not removed.");

        // Reload the page so the infobar shows again
        loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened");
        infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);
        assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        mListener.swapInfoBarAnimationFinished("InfoBar not swapped");

        TranslateUtil.assertInfoBarText(infoBar, NEVER_TRANSLATE_MESSAGE);
    }

    /**
     * Test infobar transitions.
     *
     * @MediumTest
     * @Feature({"Browser", "Main"})
     */
    @DisabledTest(message = "crbug.com/514449")
    public void testTranslateTransitions() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not Added");
        InfoBar infoBar = getInfoBars().get(0);
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBar));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBar));
        assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        mListener.swapInfoBarAnimationFinished("BEFORE -> TRANSLATING transition not Swapped.");
        mListener.swapInfoBarAnimationFinished("TRANSLATING -> ERROR transition not Swapped.");
    }
}
