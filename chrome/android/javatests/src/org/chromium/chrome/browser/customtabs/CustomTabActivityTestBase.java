// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;

import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * Base class for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public abstract class CustomTabActivityTestBase extends
        ChromeActivityTestCaseBase<CustomTabActivity> {

    public CustomTabActivityTestBase() {
        super(CustomTabActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

    @Override
    protected void startActivityCompletely(Intent intent) {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                CustomTabActivity.class.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        CustomTabActivity customTabActivity =
                (CustomTabActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("CustomTabActivity did not start", customTabActivity);
        setActivity(customTabActivity);
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    protected void startCustomTabActivityWithIntent(Intent intent) throws InterruptedException {
        startActivityCompletely(intent);
        assertTrue("Tab never selected/initialized.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().getActivityTab() != null;
                    }
                }));
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        try {
            if (tab.isLoading()) pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
        assertTrue("Deferred startup never completed",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return DeferredStartupHandler.getInstance().isDeferredStartupComplete();
                    }
                }, 5000, 200));
        assertNotNull(tab);
        assertNotNull(tab.getView());
    }
}
