// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_SHORT_MS;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.vr_shell.VrTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Class containing utility functions for interacting with InfoBars at
 * a high level.
 */
public class VrInfoBarUtils {
    public enum Button { PRIMARY, SECONDARY };

    /**
     * Determines whether InfoBars are present in the current activity.
     * @return True if there are any InfoBars present, false otherwise
     */
    public static boolean isInfoBarPresent(VrTestRule rule) {
        List<InfoBar> infoBars = rule.getInfoBars();
        return infoBars != null && !infoBars.isEmpty();
    }

    /**
     * Clicks on either the primary or secondary button of the first InfoBar
     * in the activity.
     * @param button Which button to click
     * @param rule The VrTestRule to get the current activity from
     */
    public static void clickInfoBarButton(final Button button, VrTestRule rule) {
        if (!isInfoBarPresent(rule)) return;
        final List<InfoBar> infoBars = rule.getInfoBars();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                switch (button) {
                    case PRIMARY:
                        InfoBarUtil.clickPrimaryButton(infoBars.get(0));
                        break;
                    default:
                        InfoBarUtil.clickSecondaryButton(infoBars.get(0));
                }
            }
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());
    }

    /**
     * Clicks on the close button of the first InfoBar in the activity.
     * @param rule The VrTestRule to get the current activity from
     */
    public static void clickInfobarCloseButton(VrTestRule rule) {
        if (!isInfoBarPresent(rule)) return;
        final List<InfoBar> infoBars = rule.getInfoBars();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                InfoBarUtil.clickCloseButton(infoBars.get(0));
            }
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());
    }

    /**
     * Determines is there is any InfoBar present in the given View hierarchy.
     * @param parentView The View to start the search in
     * @param present Whether an InfoBar should be present.
     */
    public static void expectInfoBarPresent(final VrTestRule rule, boolean present) {
        CriteriaHelper.pollUiThread(Criteria.equals(present, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return isInfoBarPresent(rule);
            }
        }), POLL_TIMEOUT_SHORT_MS, POLL_CHECK_INTERVAL_SHORT_MS);
    }
}
