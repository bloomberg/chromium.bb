// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.base.test.util.Feature;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Base test class for all CronetSample based tests.
 */
public class CronetSampleTest extends
        ActivityInstrumentationTestCase2<CronetSampleActivity> {

    /**
     * The maximum time the waitForActiveShellToBeDoneLoading method will wait.
     */
    private static final long
            WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);

    protected static final long
            WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    public CronetSampleTest() {
        super(CronetSampleActivity.class);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetSampleActivity activity = launchCronetSampleWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure that the URL is set as expected.
        assertEquals(URL, activity.getUrl());
        assertEquals(200, activity.getHttpStatusCode());
    }

    /**
     * Starts the CronetSample activity and loads the given URL. The URL can be
     * null, in which case will default to
     * CronetSampleActivity.DEFAULT_SHELL_URL.
     */
    protected CronetSampleActivity launchCronetSampleWithUrl(String url) {
        return launchCronetSampleWithUrlAndCommandLineArgs(url, null);
    }

    /**
     * Starts the CronetSample activity appending the provided command line
     * arguments and loads the given URL. The URL can be null, in which case
     * will default to CronetSampleActivity.DEFAULT_SHELL_URL.
     */
    protected CronetSampleActivity launchCronetSampleWithUrlAndCommandLineArgs(
            String url, String[] commandLineArgs) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null)
            intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(),
                CronetSampleActivity.class));
        if (commandLineArgs != null) {
            intent.putExtra(CronetSampleActivity.COMMAND_LINE_ARGS_KEY,
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
        final CronetSampleActivity activity = getActivity();

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
