// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;

import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.File;
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
                            TestShellTab tab = activity.getActiveTab();
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
     * Clear all files in the testshell's directory except 'lib'.
     *
     * @return Whether clearing the application data was successful.
     */
    protected boolean clearAppData() throws Exception {
        final int MAX_TIMEOUT_MS = 3000;
        final String appDir = getAppDir();
        return CriteriaHelper.pollForCriteria(
                new Criteria() {
                    private boolean mDataRemoved = false;
                    @Override
                    public boolean isSatisfied() {
                        if (!mDataRemoved && !removeAppData(appDir)) {
                            return false;
                        }
                        mDataRemoved = true;
                        // We have to make sure the cache directory still exists, as the framework
                        // will try to create it otherwise and will fail for sandbox processes with
                        // an NPE.
                        File cacheDir = new File(appDir, "cache");
                        if (cacheDir.exists()) {
                            return true;
                        }
                        return cacheDir.mkdir();
                    }
                },
                MAX_TIMEOUT_MS, MAX_TIMEOUT_MS / 10);
    }

    /**
     * Remove all files and directories under the given appDir except 'lib'
     *
     * @param appDir the app directory to remove.
     *
     * @return true if succeeded.
     */
    private boolean removeAppData(String appDir) {
        File[] files = new File(appDir).listFiles();
        if (files == null) {
            return true;
        }
        for (File file : files) {
            if (!file.getAbsolutePath().endsWith("/lib") && !removeFile(file)) {
                return false;
            }
        }
        return true;
    }

    private String getAppDir() {
        Context target_ctx = getInstrumentation().getTargetContext();
        String cacheDir = target_ctx.getCacheDir().getAbsolutePath();
        return cacheDir.substring(0, cacheDir.lastIndexOf('/'));
    }

    /**
     * Remove the given file or directory.
     *
     * @param file the file or directory to remove.
     *
     * @return true if succeeded.
     */
    private static boolean removeFile(File file) {
        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files == null) {
                return true;
            }
            for (File sub_file : files) {
                if (!removeFile(sub_file))
                    return false;
            }
        }
        return file.delete();
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
