// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

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
 * Utility class for interacting with XR-specific Rules, i.e. ChromeActivityTestRules that implement
 * the XrTestRule interface.
 */
public class XrTestRuleUtils {
    /**
     * Creates the list of XrTestRules that are currently supported for use in test
     * parameterization.
     */
    public static ArrayList<ParameterSet> generateDefaultTestRuleParameters() {
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
}
