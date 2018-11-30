// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.filters.MediumTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.init.ServiceManagerStartupUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Tests background tasks can be run in Service Manager only mode, i.e., without starting the full
 * browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public final class ServicificationBackgroundServiceTest {
    private MockTaskService mTaskService;

    static class MockTaskService extends ChromeBackgroundService {
        private boolean mDidLaunchBrowser;
        private boolean mDidStartServiceManager;

        @Override
        protected void launchBrowser(Context context, String tag) {
            mDidLaunchBrowser = true;

            final BrowserParts parts = new EmptyBrowserParts() {
                @Override
                public void finishNativeInitialization() {
                    mDidStartServiceManager = true;
                }

                @Override
                public boolean startServiceManagerOnly() {
                    return true;
                }
            };

            try {
                ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
                ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
            } catch (ProcessInitException e) {
                ChromeApplication.reportStartupErrorAndExit(e);
            }
        }

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser) {
            ThreadUtils.runOnUiThread(() -> {
                Assert.assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
            });
        }

        public void waitForServiceManagerStart() {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed while waiting for starting Service Manager.") {
                        @Override
                        public boolean isSatisfied() {
                            return mDidStartServiceManager;
                        }
                    });
        }
    }

    @Before
    public void setUp() {
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.setDisabledForTests(true);
        mTaskService = new MockTaskService();
    }

    @After
    public void tearDown() throws Exception {
        RecordHistogram.setDisabledForTests(false);
    }

    private void startOnRunTaskAndVerify(String taskTag, boolean shouldStart) {
        mTaskService.onRunTask(new TaskParams(taskTag));
        mTaskService.checkExpectations(shouldStart);
        mTaskService.waitForServiceManagerStart();
    }

    @Test
    @MediumTest
    @Feature({"ServicificationStartup"})
    @CommandLineFlags.
    Add({"enable-features=NetworkService,NetworkServiceInProcess,AllowStartingServiceManagerOnly"})
    public void testSeriveManagerStarts() {
        startOnRunTaskAndVerify(ServiceManagerStartupUtils.TASK_TAG, true);
    }
}
