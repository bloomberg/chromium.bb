// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the appearance of InfoBars.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@ScreenShooter.Directory("InfoBars")
public class InfoBarAppearanceTest {
    // clang-format on

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    private InfoBarTestAnimationListener mListener;
    private Tab mTab;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();

        mListener = new InfoBarTestAnimationListener();

        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTab.getInfoBarContainer().addAnimationListener(mListener);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "Catalogue"})
    public void testFramebustBlockInfoBar() throws Exception {
        FramebustBlockInfoBar infobar = new FramebustBlockInfoBar("http://very.evil.biz");
        captureMiniAndRegularInfobar(infobar);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "Catalogue"})
    public void testFramebustBlockInfoBarWithLongMessages() throws Exception {
        FramebustBlockInfoBar infobar = new FramebustBlockInfoBar("https://someverylonglink"
                + "thatwilldefinitelynotfitevenwhenremovingthefilepath.com/somemorestuff");
        captureMiniAndRegularInfobar(infobar);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "Catalogue"})
    public void testOomInfoBar() throws TimeoutException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTab.getInfoBarContainer().addInfoBarForTesting(new NearOomInfoBar()));
        mListener.addInfoBarAnimationFinished("InfoBar was not added.");
        mScreenShooter.shoot("oom_infobar");
    }

    private void captureMiniAndRegularInfobar(InfoBar infobar)
            throws TimeoutException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTab.getInfoBarContainer().addInfoBarForTesting(infobar));
        mListener.addInfoBarAnimationFinished("InfoBar was not added.");
        mScreenShooter.shoot("compact");

        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);
        mListener.swapInfoBarAnimationFinished("InfoBar did not expand.");
        mScreenShooter.shoot("expanded");
    }
}
