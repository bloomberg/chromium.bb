// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

/**
 * Tests {@link BackgroundSyncLauncherService}.
 */
public class BackgroundSyncLauncherServiceTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mLauncher;
    private MockLauncherService mLauncherService;

    static class MockLauncherService extends BackgroundSyncLauncherService {
        private boolean mDidStartService = false;

        @Override
        protected void launchBrowser(Context context) {
            mDidStartService = true;
        }

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedStartService) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    assertEquals("StartedService", expectedStartService, mDidStartService);
                }
            });
        }
    }

    @Override
    protected void setUp() throws Exception {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.disableForTests();
        mLauncher = BackgroundSyncLauncher.create(mContext);
        mLauncherService = new MockLauncherService();
    }

    private void deleteLauncherInstance() {
        mLauncher.destroy();
        mLauncher = null;
    }

    private void startOnRunTaskAndVerify(boolean shouldStart) {
        mLauncherService.onRunTask(null);
        mLauncherService.checkExpectations(shouldStart);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testNoFireWhenInstanceExists() {
        startOnRunTaskAndVerify(false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testFiresWhenInstanceDoesNotExist() {
        deleteLauncherInstance();
        startOnRunTaskAndVerify(true);
    }
}
