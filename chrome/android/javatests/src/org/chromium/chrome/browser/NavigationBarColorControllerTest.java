// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import android.annotation.TargetApi;
import android.graphics.Color;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageTest.ModernParams;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.ChromeModernDesign;

import java.util.Arrays;

/** Tests for the NavigationBarColorController.  */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
@TargetApi(Build.VERSION_CODES.O_MR1)
public class NavigationBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /** Parameter provider for enabling/disabling Chrome Modern. */
    public static class ModernParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(false).name("DisableChromeModern"),
                    new ParameterSet().value(true).name("EnableChromeModern"));
        }
    }

    private ChromeModernDesign.Processor mChromeModernProcessor =
            new ChromeModernDesign.Processor();

    private Window mWindow;
    private int mLightNavigationColor;
    private int mDarkNavigationColor;

    @ParameterAnnotations.UseMethodParameterBefore(ModernParams.class)
    public void setupModernDesign(boolean enabled) {
        mChromeModernProcessor.setPrefs(enabled);
    }

    @ParameterAnnotations.UseMethodParameterAfter(ModernParams.class)
    public void teardownModernDesign(boolean enabled) {
        mChromeModernProcessor.clearTestState();
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mWindow = mActivityTestRule.getActivity().getWindow();
        mLightNavigationColor = ApiCompatibilityUtils.getColor(
                mActivityTestRule.getActivity().getResources(), R.color.bottom_system_nav_color);
        mDarkNavigationColor = Color.BLACK;
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ModernParams.class)
    public void testToggleOverview(boolean modern) {
        assertEquals("Navigation bar should be white before entering overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));

        if (modern) {
            assertEquals("Navigation bar should be white in overview mode.", mLightNavigationColor,
                    mWindow.getNavigationBarColor());
        } else {
            assertEquals("Navigation bar should be black in overview mode.", mDarkNavigationColor,
                    mWindow.getNavigationBarColor());
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));

        assertEquals("Navigation bar should be white after exiting overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }

    @Test
    @SmallTest
    public void testToggleIncognito() throws InterruptedException {
        assertEquals("Navigation bar should be white on normal tabs.", mLightNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), true, true);

        assertEquals("Navigation bar should be black on incognito tabs.", mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);

        assertEquals("Navigation bar should be white after switching back to normal tab.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }
}
