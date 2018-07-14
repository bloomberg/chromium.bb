// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.rules.XrActivityRestrictionRule;
import org.chromium.chrome.browser.vr.rules.XrTestRule;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Utility class for interacting with VR-specific Rules, particularly XrActivityRestrictionRule.
 */
public class XrTestRuleUtils {
    // VrCore waits this amount of time after exiting VR before actually unregistering a registered
    // Daydream intent, meaning that it still thinks VR is active until this amount of time has
    // passed.
    private static final int VRCORE_UNREGISTER_DELAY_MS = 500;
    /**
     * Creates the list of XrTestRules that are currently supported for use in test
     * parameterization.
     */
    public static ArrayList<ParameterSet> generateDefaultXrTestRuleParameters() {
        ArrayList<ParameterSet> parameters = new ArrayList<ParameterSet>();
        parameters.add(new ParameterSet()
                               .value(new Callable<ChromeTabbedActivityXrTestRule>() {
                                   @Override
                                   public ChromeTabbedActivityXrTestRule call() {
                                       return new ChromeTabbedActivityXrTestRule();
                                   }
                               })
                               .name("ChromeTabbedActivity"));

        parameters.add(new ParameterSet()
                               .value(new Callable<CustomTabActivityXrTestRule>() {
                                   @Override
                                   public CustomTabActivityXrTestRule call() {
                                       return new CustomTabActivityXrTestRule();
                                   }
                               })
                               .name("CustomTabActivity"));

        parameters.add(new ParameterSet()
                               .value(new Callable<WebappActivityXrTestRule>() {
                                   @Override
                                   public WebappActivityXrTestRule call() {
                                       return new WebappActivityXrTestRule();
                                   }
                               })
                               .name("WebappActivity"));

        return parameters;
    }

    /**
     * Creates a RuleChain that applies the XrActivityRestrictionRule before the given XrTestRule.
     */
    public static RuleChain wrapRuleInXrActivityRestrictionRule(TestRule rule) {
        Assert.assertTrue("Given rule is a XrTestRule", rule instanceof XrTestRule);
        return RuleChain
                .outerRule(new XrActivityRestrictionRule(((XrTestRule) rule).getRestriction()))
                .around(rule);
    }

    /**
     * Converts XrActivityRestriction.SupportedActivity enum to strings
     */
    public static String supportedActivityToString(SupportedActivity activity) {
        switch (activity) {
            case CTA:
                return "ChromeTabbedActivity";
            case CCT:
                return "CustomTabActivity";
            case WAA:
                return "WebappActivity";
            case ALL:
                return "AllActivities";
            default:
                return "UnknownActivity";
        }
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
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String currentPackageName = uiDevice.getCurrentPackageName();
        if (currentPackageName != null && currentPackageName.contains("vr")) {
            uiDevice.pressHome();
            // Chrome startup would likely be slow enough that this sleep is unnecessary, but sleep
            // to be sure since this will be hit relatively infrequently.
            SystemClock.sleep(VRCORE_UNREGISTER_DELAY_MS);
        }
    }
}
