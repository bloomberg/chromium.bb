// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.content.Intent;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.BundleUtils;
import org.chromium.base.test.BundleTestRule;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.VrFeedbackStatus;
import org.chromium.chrome.browser.vr.VrIntentDelegate;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityVrTestRule;
import org.chromium.chrome.browser.vr.rules.VrActivityRestrictionRule;
import org.chromium.chrome.browser.vr.rules.VrTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityVrTestRule;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Utility class for interacting with VR-specific Rules, i.e. ChromeActivityTestRules that implement
 * the VrTestRule interface.
 */
public class VrTestRuleUtils extends XrTestRuleUtils {
    // VrCore waits this amount of time after exiting VR before actually unregistering a registered
    // Daydream intent, meaning that it still thinks VR is active until this amount of time has
    // passed.
    private static final int VRCORE_UNREGISTER_DELAY_MS = 500;

    /**
     * Helper method to apply a VrTestRule/ChromeActivityTestRule combination. The only difference
     * between various classes that implement VrTestRule is how they start their activity, so the
     * common boilerplate code can be kept here so each VrTestRule only has to provide a way to
     * launch Chrome.
     *
     * @param base The Statement passed to the calling ChromeActivityTestRule's apply() method.
     * @param desc The Description passed to the calling ChromeActivityTestRule's apply() method.
     * @param rule The calling VrTestRule.
     * @param launcher A ChromeLaunchMethod whose launch() contains the code snippet to start Chrome
     *        in the calling ChromeActivityTestRule's activity type.
     */
    public static void evaluateVrTestRuleImpl(final Statement base, final Description desc,
            final VrTestRule rule, final ChromeLaunchMethod launcher) throws Throwable {
        TestVrShellDelegate.setDescription(desc);

        VrTestRuleUtils.ensureNoVrActivitiesDisplayed();
        launcher.launch();
        // Must be called after Chrome is started, as otherwise startService fails with an
        // IllegalStateException for being used from a backgrounded app.
        VrSettingsServiceUtils.checkForAndApplyVrSettingsFileAnnotation(desc, rule);

        // Reset the VR feedback shared preferences if they're not currently the default because
        // otherwise we can run into issues with VrFeedbackInfoBarTest#* erroneously failing due to
        // tests being non-hermetic. Only set if not the default instead of unconditionally in order
        // to avoid unnecessary disk writes.
        if (VrFeedbackStatus.getFeedbackOptOut()) {
            VrFeedbackStatus.setFeedbackOptOut(false);
        }
        if (VrFeedbackStatus.getUserExitedAndEntered2DCount() != 0) {
            VrFeedbackStatus.setUserExitedAndEntered2DCount(0);
        }

        base.evaluate();
    }

    /**
     * Creates the list of VrTestRules that are currently supported for use in test
     * parameterization.
     *
     * @return An ArrayList of ParameterSets, with each ParameterSet containing a callable to create
     *         a VrTestRule for a supported ChromeActivity.
     */
    public static ArrayList<ParameterSet> generateDefaultTestRuleParameters() {
        ArrayList<ParameterSet> parameters = new ArrayList<ParameterSet>();
        parameters.add(new ParameterSet()
                               .value(new Callable<ChromeTabbedActivityVrTestRule>() {
                                   @Override
                                   public ChromeTabbedActivityVrTestRule call() {
                                       return new ChromeTabbedActivityVrTestRule();
                                   }
                               })
                               .name("ChromeTabbedActivity"));

        parameters.add(new ParameterSet()
                               .value(new Callable<CustomTabActivityVrTestRule>() {
                                   @Override
                                   public CustomTabActivityVrTestRule call() {
                                       return new CustomTabActivityVrTestRule();
                                   }
                               })
                               .name("CustomTabActivity"));

        parameters.add(new ParameterSet()
                               .value(new Callable<WebappActivityVrTestRule>() {
                                   @Override
                                   public WebappActivityVrTestRule call() {
                                       return new WebappActivityVrTestRule();
                                   }
                               })
                               .name("WebappActivity"));

        return parameters;
    }

    /**
     * Creates a RuleChain that applies the XrActivityRestrictionRule and VrActivityRestrictionRule
     * before the given VrTestRule.
     *
     * Also enforces that {@link BundleUtils#isBundle()} returns true for vr to be initialized.
     *
     * @param rule The TestRule to wrap in an XrActivityRestrictionRule and
     *        VrActivityRestrictionRule.
     * @return A RuleChain that ensures an XrActivityRestrictionRule and VrActivityRestrictionRule
     *         is applied before the provided TestRule.
     */
    public static RuleChain wrapRuleInActivityRestrictionRule(TestRule rule) {
        Assert.assertTrue("Given rule is not an VrTestRule", rule instanceof VrTestRule);
        return RuleChain
                .outerRule(new VrActivityRestrictionRule(((VrTestRule) rule).getRestriction()))
                .around(new BundleTestRule())
                .around(XrTestRuleUtils.wrapRuleInActivityRestrictionRule(rule));
    }

    /**
     * Ensures that no VR-related activity is currently being displayed. This is meant to be used
     * by TestRules before starting any activities. Having a VR activity in the foreground (e.g.
     * Daydream Home) has the potential to affect test results, as it often means that we are in VR
     * at the beginning of the test, which we don't want. This is most commonly caused by VrCore
     * automatically launching Daydream Home when Chrome gets closed after a test, but can happen
     * for other reasons as well.
     */
    public static void ensureNoVrActivitiesDisplayed() {
        // This will always be hit on standalones, but we're expected to be in VR in that case.
        if (TestVrShellDelegate.isOnStandalone()) return;
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String currentPackageName = uiDevice.getCurrentPackageName();
        if (currentPackageName != null && currentPackageName.contains("vr")) {
            uiDevice.pressHome();
            // Chrome startup would likely be slow enough that this sleep is unnecessary, but sleep
            // to be sure since this will be hit relatively infrequently.
            SystemClock.sleep(VRCORE_UNREGISTER_DELAY_MS);
        }
    }

    /**
     * Helper method to add additional data to a Chrome startup intent that makes it usable on a
     * standalone VR device.
     */
    public static Intent maybeAddStandaloneIntentData(Intent intent) {
        if (TestVrShellDelegate.isOnStandalone()) {
            // Tell VrShellDelegate that it should create a TestVrShellDelegate on startup
            TestVrShellDelegate.enableTestVrShellDelegateOnStartupForTesting();
            intent.addCategory(VrIntentDelegate.DAYDREAM_CATEGORY);
            intent.putExtra("android.intent.extra.VR_LAUNCH", true);
        }
        return intent;
    }
}
