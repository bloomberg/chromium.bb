// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.base.test.util.UrlUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Base test class for all CronetSample based tests.
 */
public class CronetSampleTestBase extends
        ActivityInstrumentationTestCase2<CronetSampleActivity> {

    /**
     * The maximum time the waitForActiveShellToBeDoneLoading method will wait.
     */
    private static final long
            WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);

    protected static final long
            WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    public CronetSampleTestBase() {
        super(CronetSampleActivity.class);
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

    // TODO(cjhopman): These functions are inconsistent with
    // launchCronetSample***. Should be startCronetSample*** and should use the
    // url exactly without the getTestFileUrl call. Possibly these two ways of
    // starting the activity (launch* and start*) should be merged into one.
    /**
     * Starts the content shell activity with the provided test url. The url is
     * synchronously loaded.
     *
     * @param url Test url to load.
     */
    protected void startActivityWithTestUrl(String url) throws Throwable {
        launchCronetSampleWithUrl(UrlUtils.getTestFileUrl(url));
        assertNotNull(getActivity());
        assertTrue(waitForActiveShellToBeDoneLoading());
        assertEquals(UrlUtils.getTestFileUrl(url), getActivity().getUrl());
    }

    /**
     * Starts the content shell activity with the provided test url and optional
     * command line arguments to append. The url is synchronously loaded.
     *
     * @param url Test url to load.
     * @param commandLineArgs Optional command line args to append when
     *            launching the activity.
     */
    protected void startActivityWithTestUrlAndCommandLineArgs(String url,
            String[] commandLineArgs) throws Throwable {
        launchCronetSampleWithUrlAndCommandLineArgs(
                UrlUtils.getTestFileUrl(url), commandLineArgs);
        assertNotNull(getActivity());
        assertTrue(waitForActiveShellToBeDoneLoading());
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
