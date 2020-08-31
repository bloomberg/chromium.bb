// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.graphics.PointF;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.keyboard.GvrKeyboardLoaderClient;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrInfoBarUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for the InfoBar that prompts the user to update or install
 * VrCore (VR Services) when attempting to use a VR feature with an outdated
 * or entirely missing version or other VR-related update prompts.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebVR is only supported on L+
@Restriction(RESTRICTION_TYPE_SVR)
public class VrInstallUpdateInfoBarTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;

    private VrBrowserTestFramework mVrBrowserTestFramework;

    public VrInstallUpdateInfoBarTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mVrTestRule);
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
    }

    /**
     * Helper function to run the tests checking for the upgrade/install InfoBar being present since
     * all that differs is the value returned by VrCoreVersionChecker and a couple asserts.
     *
     * @param checkerReturnCompatibility The compatibility to have the VrCoreVersionChecker return.
     */
    private void infoBarTestHelper(final int checkerReturnCompatibility) {
        VrShellDelegateUtils.setVrCoreCompatibility(checkerReturnCompatibility);
        View decorView = mVrTestRule.getActivity().getWindow().getDecorView();
        if (checkerReturnCompatibility == VrCoreCompatibility.VR_READY) {
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, false);
        } else if (checkerReturnCompatibility == VrCoreCompatibility.VR_OUT_OF_DATE
                || checkerReturnCompatibility == VrCoreCompatibility.VR_NOT_AVAILABLE) {
            // Out of date and missing cases are the same, but with different text
            String expectedMessage;
            String expectedButton;
            if (checkerReturnCompatibility == VrCoreCompatibility.VR_OUT_OF_DATE) {
                expectedMessage = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_update_text);
                expectedButton = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_update_button);
            } else {
                expectedMessage = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_install_text);
                expectedButton = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_install_button);
            }
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, true);
            TextView tempView = (TextView) decorView.findViewById(R.id.infobar_message);
            Assert.assertEquals("VR install/update infobar text did not match expectation",
                    expectedMessage, tempView.getText().toString());
            tempView = (TextView) decorView.findViewById(R.id.button_primary);
            Assert.assertEquals("VR install/update button text did not match expectation",
                    expectedButton, tempView.getText().toString());
        } else if (checkerReturnCompatibility == VrCoreCompatibility.VR_NOT_SUPPORTED) {
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, false);
        } else {
            Assert.fail("Invalid VrCoreVersionChecker compatibility: "
                    + String.valueOf(checkerReturnCompatibility));
        }
        VrShellDelegateUtils.getDelegateInstance().overrideVrCoreVersionCheckerForTesting(null);
    }

    /**
     * Tests that the upgrade/install VR Services InfoBar is not present when VR Services is
     * installed and up to date.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testInfoBarNotPresentWhenVrServicesCurrent() {
        infoBarTestHelper(VrCoreCompatibility.VR_READY);
    }

    /**
     * Tests that the upgrade VR Services InfoBar is present when VR Services is outdated.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA,
            XrActivityRestriction.SupportedActivity.CCT})
    public void
    testInfoBarPresentWhenVrServicesOutdated() {
        infoBarTestHelper(VrCoreCompatibility.VR_OUT_OF_DATE);
    }

    /**
     * Tests that the install VR Services InfoBar is present when VR Services is missing.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA,
            XrActivityRestriction.SupportedActivity.CCT})
    public void
    testInfoBarPresentWhenVrServicesMissing() {
        infoBarTestHelper(VrCoreCompatibility.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services InfoBar is not present when VR is not supported on the
     * device.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testInfoBarNotPresentWhenVrServicesNotSupported() {
        infoBarTestHelper(VrCoreCompatibility.VR_NOT_SUPPORTED);
    }

    /**
     * Tests that the install/upgrade prompt for the keyboard appears when clicking on the URL
     * bar without the keyboard installed.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testKeyboardInstallUpgradePromptUrlBar() {
        testKeyboardInstallUpgradeImpl(UserFriendlyElementName.URL);
    }

    /**
     * Tests that the install/upgrade prompt for the keyboard appears when interacting with a web
     * text input field without the keyboard installed.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testKeyboardInstallUpgradePromptWebInput() {
        testKeyboardInstallUpgradeImpl(UserFriendlyElementName.CONTENT_QUAD);
    }

    private void testKeyboardInstallUpgradeImpl(final int uiElementToClick) {
        mVrTestRule.loadUrl(mVrBrowserTestFramework.getUrlForFile("test_web_input_editing"),
                PAGE_LOAD_TIMEOUT_S);
        GvrKeyboardLoaderClient.setFailLoadForTesting(true);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // The prompt takes significantly longer to show when clicking on the web content, so we
        // can't just wait for quiescence since that gets reached before the prompt shows (not sure
        // what's causing a UI change other than the prompt). Instead, explicitly wait for the
        // prompt to become visible before waiting for quiescence.
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.EXIT_PROMPT, true /* visible */,
                    () -> { NativeUiUtils.clickElement(uiElementToClick, new PointF()); });
        });
    }
}
