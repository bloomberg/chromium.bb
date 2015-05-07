// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.base.PathUtils;

import java.io.File;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Method;
import java.net.URL;
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
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(),
                CronetTestActivity.class));
        if (commandLineArgs != null) {
            intent.putExtra(CronetTestActivity.COMMAND_LINE_ARGS_KEY,
                    commandLineArgs);
        }
        setActivityIntent(intent);
        // Make sure the activity was created as expected.
        assertNotNull(getActivity());
        try {
            waitForActivityToBeDoneLoading();
        } catch (Throwable e) {
            fail("Test activity has failed to load.");
        }
        return getActivity();
    }

    // Helper method to tell the activity to skip factory init in onCreate().
    protected CronetTestActivity skipFactoryInitInOnCreate() {
        String[] commandLineArgs = {
                CronetTestActivity.LIBRARY_INIT_KEY, CronetTestActivity.LIBRARY_INIT_SKIP};
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(null, commandLineArgs);
        return activity;
    }

    /**
     * Waits for the Activity to finish loading. This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds.
     *
     * @return Whether or not the Shell was actually finished loading.
     * @throws InterruptedException
     */
    private boolean waitForActivityToBeDoneLoading()
            throws InterruptedException {
        final CronetTestActivity activity = getActivity();

        // Wait for the Activity to load.
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    final AtomicBoolean isLoaded = new AtomicBoolean(false);
                    runTestOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            isLoaded.set(activity != null && !activity.isLoading());
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

    @Override
    protected void runTest() throws Throwable {
        if (!getClass().getPackage().getName().equals(
                "org.chromium.net.urlconnection")) {
            super.runTest();
            return;
        }
        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            if (method.isAnnotationPresent(CompareDefaultWithCronet.class)) {
                // Run with the default HttpURLConnection implementation first.
                super.runTest();
                // Use Cronet's implementation, and run the same test.
                URL.setURLStreamHandlerFactory(
                        getActivity().mStreamHandlerFactory);
                super.runTest();
            } else if (method.isAnnotationPresent(
                    OnlyRunCronetHttpURLConnection.class)) {
                // Run only with Cronet's implementation.
                URL.setURLStreamHandlerFactory(
                        getActivity().mStreamHandlerFactory);
                super.runTest();
            } else {
                // For all other tests.
                super.runTest();
            }
        } catch (Throwable e) {
            throw new Throwable("CronetTestBase#runTest failed.", e);
        }
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     */
    public String prepareTestStorage() {
        String storagePath = PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()) + "/test_storage";
        File storage = new File(storagePath);
        if (storage.exists()) {
            assertTrue(recursiveDelete(storage));
        }
        assertTrue(storage.mkdir());
        return storagePath;
    }

    boolean recursiveDelete(File path) {
        if (path.isDirectory()) {
            for (File c : path.listFiles()) {
                if (!recursiveDelete(c)) {
                    return false;
                }
            }
        }
        return path.delete();
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface CompareDefaultWithCronet {
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunCronetHttpURLConnection {
    }

}
