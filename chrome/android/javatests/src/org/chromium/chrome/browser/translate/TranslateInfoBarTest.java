// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class TranslateInfoBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String DISABLE_COMPACT_UI_FEATURE = "disable-features=TranslateCompactUI";
    private static final String NEVER_TRANSLATE_MESSAGE =
            "Would you like Google Chrome to offer to translate French pages from this"
                    + " site next time?";

    private InfoBarContainer mInfoBarContainer;
    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mInfoBarContainer = mActivityTestRule.getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        mInfoBarContainer.addAnimationListener(mListener);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test the translate language panel.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    @CommandLineFlags.Add(DISABLE_COMPACT_UI_FEATURE)
    public void testTranslateLanguagePanel() throws InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        InfoBar infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBar));
        Assert.assertTrue(InfoBarUtil.hasSecondaryButton(infoBar));
        TranslateUtil.openLanguagePanel(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), infoBar);
    }

    /**
     * Test the "never translate" panel.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    @CommandLineFlags.Add(DISABLE_COMPACT_UI_FEATURE)
    public void testTranslateNeverPanel() throws InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        InfoBar infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);

        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        mListener.removeInfoBarAnimationFinished("Infobar not removed.");

        // Reload the page so the infobar shows again
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened");
        infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        mListener.swapInfoBarAnimationFinished("InfoBar not swapped");

        TranslateUtil.assertInfoBarText(infoBar, NEVER_TRANSLATE_MESSAGE);
    }

    /**
     * Test infobar transitions.
     *
     * @MediumTest
     * @Feature({"Browser", "Main"})
     */
    @Test
    @DisabledTest(message = "crbug.com/514449")
    public void testTranslateTransitions() throws InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not Added");
        InfoBar infoBar = mActivityTestRule.getInfoBars().get(0);
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBar));
        Assert.assertTrue(InfoBarUtil.hasSecondaryButton(infoBar));
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        mListener.swapInfoBarAnimationFinished("BEFORE -> TRANSLATING transition not Swapped.");
        mListener.swapInfoBarAnimationFinished("TRANSLATING -> ERROR transition not Swapped.");
    }
}
