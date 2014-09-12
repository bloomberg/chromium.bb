// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Base test class for all CronetTest based tests.
 */
public class CronetTestBase extends
        ActivityInstrumentationTestCase2<CronetTestActivity> {
    /**
     * The maximum time the waitForActiveShellToBeDoneLoading method will wait.
     */
    private static final long
            WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);

    protected static final long
            WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    public CronetTestBase() {
        super(CronetTestActivity.class);
    }

    /**
     * Starts the CronetTest activity.
     */
    protected CronetTestActivity launchCronetTestApp() {
        return launchCronetTestAppWithUrlAndCommandLineArgs(null, null);
    }

    /**
     * Starts the CronetTest activity and loads the given URL. The URL can be
     * null.
     */
    protected CronetTestActivity launchCronetTestAppWithUrl(String url) {
        return launchCronetTestAppWithUrlAndCommandLineArgs(url, null);
    }

    /**
     * Starts the CronetTest activity appending the provided command line
     * arguments and loads the given URL. The URL can be null.
     */
    protected CronetTestActivity launchCronetTestAppWithUrlAndCommandLineArgs(
            String url, String[] commandLineArgs) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null)
            intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(),
                CronetTestActivity.class));
        if (commandLineArgs != null) {
            intent.putExtra(CronetTestActivity.COMMAND_LINE_ARGS_KEY,
                    commandLineArgs);
        }
        setActivityIntent(intent);
        return getActivity();
    }

    /**
     * Waits for the Active shell to finish loading. This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be
     * used for long loading pages. Instead it should be used more for test
     * initialization. The proper way to wait is to use a
     * TestCallbackHelperContainer after the initial load is completed.
     *
     * @return Whether or not the Shell was actually finished loading.
     * @throws InterruptedException
     */
    protected boolean waitForActiveShellToBeDoneLoading()
            throws InterruptedException {
        final CronetTestActivity activity = getActivity();

        // Wait for the Content Shell to be initialized.
        return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
            public boolean isSatisfied() {
                try {
                    final AtomicBoolean isLoaded = new AtomicBoolean(false);
                    runTestOnUiThread(new Runnable() {
                            @Override
                        public void run() {
                            if (activity != null) {
                                // There are two cases here that need to be
                                // accounted for.
                                // The first is that we've just created a Shell
                                // and it isn't
                                // loading because it has no URL set yet. The
                                // second is that
                                // we've set a URL and it actually is loading.
                                isLoaded.set(!activity.isLoading() && !TextUtils
                                        .isEmpty(activity.getUrl()));
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
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
