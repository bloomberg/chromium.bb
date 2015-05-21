// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.test.FlakyTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;

/**
 * Tests for the translate infobar, assumes it runs on a system with language
 * preferences set to English.
 *
 * TODO(newt): merge this with TranslateInfoBarTest after upstreaming.
 */
public class TranslateInfoBarTest2 extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TRANSLATE_PAGE = "chrome/test/data/translate/fr_test.html";
    private InfoBarTestAnimationListener mListener;

    public TranslateInfoBarTest2() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        InfoBarContainer container =
                getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
    }

    /**
     * Test infobar transitions.
     */
    /*
     * Bug http://crbug.com/267079.
     * @MediumTest
     */
    @FlakyTest
    @Feature({"Browser", "Main"})
    public void testInfoBarTranslate() throws InterruptedException {
        loadUrl(TestHttpServerClient.getUrl(TRANSLATE_PAGE));
        assertTrue("InfoBar not Added", mListener.addInfoBarAnimationFinished());
        InfoBar infoBar = getInfoBars().get(0);
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBar));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBar));
        assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        assertTrue("BEFORE -> TRANSLATING transition not Swapped.",
                mListener.swapInfoBarAnimationFinished());
        assertTrue("TRANSLATING -> ERROR transition not Swapped.",
                mListener.swapInfoBarAnimationFinished());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }
}
