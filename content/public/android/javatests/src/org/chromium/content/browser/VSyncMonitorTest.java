// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.support.test.filters.MediumTest;
import android.test.InstrumentationTestCase;
import android.view.WindowManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.ui.VSyncMonitor;

import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * Tests VSyncMonitor to make sure it generates correct VSync timestamps.
 */
public class VSyncMonitorTest extends InstrumentationTestCase {
    private static final int FRAME_COUNT = 60;

    private static class VSyncDataCollector implements VSyncMonitor.Listener {
        public long mFramePeriods[];
        public int mFrameCount;

        private boolean mDone;
        private long mPreviousVSyncTimeMicros;
        private final Object mSyncRoot = new Object();

        VSyncDataCollector(int frames) {
            mFramePeriods = new long[frames - 1];
        }

        public boolean isDone() {
            synchronized (mSyncRoot) {
                return mDone;
            }
        }

        @Override
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
            ThreadUtils.assertOnUiThread();
            if (mPreviousVSyncTimeMicros == 0) {
                mPreviousVSyncTimeMicros = vsyncTimeMicros;
            } else {
                mFramePeriods[mFrameCount++] = vsyncTimeMicros - mPreviousVSyncTimeMicros;
                mPreviousVSyncTimeMicros = vsyncTimeMicros;
            }
            if (mFrameCount >= mFramePeriods.length) {
                synchronized (mSyncRoot) {
                    mDone = true;
                    mSyncRoot.notify();
                }
                return;
            }
            monitor.requestUpdate();
        }

        public void waitTillDone() throws InterruptedException {
            synchronized (mSyncRoot) {
                while (!isDone()) {
                    mSyncRoot.wait();
                }
            }
        }
    }

    // The vsync monitor must be created on the UI thread to avoid associating the underlying
    // Choreographer with the Looper from the test runner thread.
    private VSyncMonitor createVSyncMonitor(final VSyncMonitor.Listener listener) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<VSyncMonitor>() {
            @Override
            public VSyncMonitor call() {
                Context context = getInstrumentation().getContext();
                return new VSyncMonitor(context, listener);
            }
        });
    }

    // Vsync requests should be made on the same thread as that used to create the VSyncMonitor (the
    // UI thread).
    private void requestVSyncMonitorUpdate(final VSyncMonitor monitor) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                monitor.requestUpdate();
            }
        });
    }

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    @MediumTest
    @DisabledTest(message = "crbug.com/674129")
    public void testVSyncPeriod() throws InterruptedException {
        // Collect roughly one second of data on a 60 fps display.
        VSyncDataCollector collector = new VSyncDataCollector(FRAME_COUNT);
        VSyncMonitor monitor = createVSyncMonitor(collector);

        long reportedFramePeriod = monitor.getVSyncPeriodInMicroseconds();
        assertTrue(reportedFramePeriod > 0);

        assertFalse(collector.isDone());
        requestVSyncMonitorUpdate(monitor);
        collector.waitTillDone();
        assertTrue(collector.isDone());

        // Check that the median frame rate is within 10% of the reported frame period.
        assertTrue(collector.mFrameCount == FRAME_COUNT - 1);
        Arrays.sort(collector.mFramePeriods, 0, collector.mFramePeriods.length);
        long medianFramePeriod = collector.mFramePeriods[collector.mFramePeriods.length / 2];
        if (Math.abs(medianFramePeriod - reportedFramePeriod) > reportedFramePeriod * .1) {
            fail("Measured median frame period " + medianFramePeriod
                    + " differs by more than 10% from the reported frame period "
                    + reportedFramePeriod + " for requested frames");
        }

        Context context = getInstrumentation().getContext();
        float refreshRate = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay().getRefreshRate();
        if (refreshRate < 30.0f) {
            // Reported refresh rate is most likely incorrect.
            // Estimated vsync period is expected to be lower than (1000000 / 30) microseconds
            assertTrue(monitor.getVSyncPeriodInMicroseconds() < 1000000 / 30);
        }
    }
}
