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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.interventions.FramebustBlockMessageDelegate;
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
    public void testNotifyInfoBar() throws Exception {
        TestFramebustBlockMessageDelegate messageDelegate = new TestFramebustBlockMessageDelegate();
        FramebustBlockInfoBar infobar = new FramebustBlockInfoBar(messageDelegate);

        captureMiniAndRegularInfobar(infobar, messageDelegate);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "Catalogue"})
    public void testNotifyInfoBarWithLongMessages() throws Exception {
        TestFramebustBlockMessageDelegate messageDelegate =
                new TestFramebustBlockMessageDelegate() {
                    @Override
                    public String getShortMessage() {
                        // Use a long message in the mini-infobar state to verify the behaviour with
                        // a longer text or for more verbose languages.
                        return getLongMessage();
                    }

                    @Override
                    public String getBlockedUrl() {
                        return "https://someverylonglinkthatwilldefinitelynotfitevenwhenremoving"
                                + "thefilepath.com/somemorestuff";
                    }
                };
        FramebustBlockInfoBar infobar = new FramebustBlockInfoBar(messageDelegate);

        captureMiniAndRegularInfobar(infobar, messageDelegate);
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

    private void captureMiniAndRegularInfobar(
            InfoBar infobar, TestFramebustBlockMessageDelegate delegate)
            throws TimeoutException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTab.getInfoBarContainer().addInfoBarForTesting(infobar));
        mListener.addInfoBarAnimationFinished("InfoBar was not added.");
        mScreenShooter.shoot("compact");

        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);
        mListener.swapInfoBarAnimationFinished("InfoBar did not expand.");
        mScreenShooter.shoot("expanded");

        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);
        delegate.linkTappedHelper.waitForCallback("link was not tapped.", 0);
    }

    private static class TestFramebustBlockMessageDelegate
            implements FramebustBlockMessageDelegate {
        public final CallbackHelper linkTappedHelper = new CallbackHelper();

        @Override
        public String getLongMessage() {
            return "This is the long description for a notify infobar. FYI stuff happened.";
        }

        @Override
        public String getShortMessage() {
            return "Stuff happened.";
        }

        @Override
        public String getBlockedUrl() {
            return "http://very.evil.biz";
        }

        @Override
        public int getIconResourceId() {
            return R.drawable.star_green;
        }

        @Override
        public void onLinkTapped() {
            linkTappedHelper.notifyCalled();
        }
    }
}
