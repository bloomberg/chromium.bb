// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.test.FlakyTest;

import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.TranslateUtil;

import java.util.List;

/**
 * Tests for the translate infobar, assumes it runs on a system with language
 * preferences set to English.
 */
public class TranslateInfoBarTest extends ChromeShellTestBase {
    private static final String TRANSLATE_PAGE = "chrome/test/data/translate/fr_test.html";

    private InfoBarTestAnimationListener mListener;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        InfoBarContainer container = getActivity().getActiveTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
    }

    private static final String NEVER_TRANSLATE_MESSAGE =
        "Would you like Google Chrome to offer to translate French pages from this" +
        " site next time?";

    /**
     * Test the translate language panel.
     * @MediumTest
     * @Feature({"Browser", "Main"})
     * http://crbug.com/311197
     */
    @FlakyTest
    public void testTranslateLanguagePanel() throws InterruptedException {
        List<InfoBar> infoBars = getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        loadUrlWithSanitization(TestHttpServerClient.getUrl(TRANSLATE_PAGE));
        assertTrue("InfoBar not opened.", mListener.addInfoBarAnimationFinished());
        InfoBar infoBar = infoBars.get(0);
        assertTrue(InfoBarUtil.hasPrimaryButton(this, infoBar));
        assertTrue(InfoBarUtil.hasSecondaryButton(this, infoBar));
        assertTrue("Language Panel not opened.", TranslateUtil.openLanguagePanel(this, infoBar));
    }


    /**
     * Test the "never translate" panel.
     * @MediumTest
     * @Feature({"Browser", "Main"})
     * http://crbug.com/311197
     */
    @FlakyTest
    public void testTranslateNeverPanel() throws InterruptedException {
        List<InfoBar> infoBars = getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        loadUrlWithSanitization(TestHttpServerClient.getUrl(TRANSLATE_PAGE));
        assertTrue("InfoBar not opened.", mListener.addInfoBarAnimationFinished());
        InfoBar infoBar = infoBars.get(0);

        assertTrue(InfoBarUtil.clickCloseButton(this, infoBar));
        assertTrue(mListener.removeInfoBarAnimationFinished());

        // Reload the page so the infobar shows again
        loadUrlWithSanitization(TestHttpServerClient.getUrl(TRANSLATE_PAGE));
        assertTrue("InfoBar not opened", mListener.addInfoBarAnimationFinished());
        infoBar = infoBars.get(0);
        assertTrue(InfoBarUtil.clickCloseButton(this, infoBar));

        assertTrue("Never Panel not opened.",
                TranslateUtil.verifyInfoBarText(infoBar, NEVER_TRANSLATE_MESSAGE));
    }
}
