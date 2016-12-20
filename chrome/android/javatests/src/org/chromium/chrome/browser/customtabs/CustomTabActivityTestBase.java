// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Base class for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public abstract class CustomTabActivityTestBase extends
        ChromeActivityTestCaseBase<CustomTabActivity> {

    protected static final long STARTUP_TIMEOUT_MS = scaleTimeout(5) * 1000;
    protected static final long LONG_TIMEOUT_MS = scaleTimeout(10) * 1000;

    public CustomTabActivityTestBase() {
        super(CustomTabActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

    @Override
    protected void startActivityCompletely(Intent intent) {
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<WeakReference<Activity>> list = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> ref : list) {
                    Activity activity = ref.get();
                    if (activity == null) continue;
                    if (activity instanceof CustomTabActivity) {
                        setActivity(activity);
                        return true;
                    }
                }
                return false;
            }
        });
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    protected void startCustomTabActivityWithIntent(Intent intent) throws InterruptedException {
        startActivityCompletely(intent);
        CriteriaHelper.pollUiThread(new Criteria("Tab never selected/initialized.") {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab() != null;
            }
        });
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        try {
            if (tab.isLoading()) {
                pageLoadFinishedHelper.waitForCallback(0, 1, LONG_TIMEOUT_MS,
                        TimeUnit.MILLISECONDS);
            }
        } catch (TimeoutException e) {
            fail();
        }
        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        }, STARTUP_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        assertNotNull(tab);
        assertNotNull(tab.getView());
    }
}
