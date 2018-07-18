// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityVrTestRule;
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
     * Creates the list of VrTestRules that are currently supported for use in test
     * parameterization.
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
