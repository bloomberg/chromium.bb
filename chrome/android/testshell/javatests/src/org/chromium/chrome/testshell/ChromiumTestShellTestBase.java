// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;

import org.chromium.chrome.browser.TabBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Base test class for all ChromiumTestShell based tests.
 */
public class ChromiumTestShellTestBase extends
        ActivityInstrumentationTestCase2<ChromiumTestShellActivity> {
    /** The maximum time the waitForActiveShellToBeDoneLoading method will wait. */
    private static final long WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = 10000;

    public ChromiumTestShellTestBase() {
        super(ChromiumTestShellActivity.class);
    }

    /**
     * Starts the ChromiumTestShell activity and loads the given URL.
     */
    protected ChromiumTestShellActivity launchChromiumTestShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromiumTestShellActivity.class));
        setActivityIntent(intent);
        return getActivity();
    }

    /**
     * Starts the ChromiumTestShell activity and loads a blank page.
     */
    protected ChromiumTestShellActivity launchChromiumTestShellWithBlankPage() {
        return launchChromiumTestShellWithUrl("about:blank");
    }

    /**
     * Waits for the Active shell to finish loading.  This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be used for long
     * loading pages. Instead it should be used more for test initialization. The proper way
     * to wait is to use a TestCallbackHelperContainer after the initial load is completed.
     * @return Whether or not the Shell was actually finished loading.
     * @throws Exception
     */
    protected boolean waitForActiveShellToBeDoneLoading() throws InterruptedException {
        final ChromiumTestShellActivity activity = getActivity();

        // Wait for the Content Shell to be initialized.
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    final AtomicBoolean isLoaded = new AtomicBoolean(false);
                    runTestOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            TabBase tab = activity.getActiveTab();
                            if (tab != null) {
                                isLoaded.set(!tab.isLoading()
                                        && !TextUtils.isEmpty(tab.getContentView().getUrl()));
                            } else {
                                isLoaded.set(false);
                            }
                        }
                    });

                    return isLoaded.get();
                } catch (Throwable e) {
                    return false;
                }
            }
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Navigates the currently active tab to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     */
    public void loadUrlWithSanitization(final String url) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveTab().loadUrlWithSanitization(url);
            }
        });
        waitForActiveShellToBeDoneLoading();
    }
}
