// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import java.util.Arrays;
import java.util.concurrent.Callable;

import org.chromium.base.ThreadUtils;

public class VSyncMonitorTest extends InstrumentationTestCase {
    private static class VSyncDataCollector implements VSyncMonitor.Listener {
        public long mFramePeriods[];
        public int mFrameCount;

        private boolean mDone;
        private long mPreviousVSyncTimeMicros;
        private Object mSyncRoot = new Object();

        VSyncDataCollector(int frames) {
            mFramePeriods = new long[frames];
        }

        public boolean isDone() {
            synchronized (mSyncRoot) {
                return mDone;
            }
        }

        @Override
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
            if (mPreviousVSyncTimeMicros == 0) {
                mPreviousVSyncTimeMicros = vsyncTimeMicros;
                return;
            }
            if (mFrameCount >= mFramePeriods.length) {
                synchronized (mSyncRoot) {
                    mDone = true;
                    mSyncRoot.notify();
                }
                return;
            }
            mFramePeriods[mFrameCount++] = vsyncTimeMicros - mPreviousVSyncTimeMicros;
            mPreviousVSyncTimeMicros = vsyncTimeMicros;
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
    private VSyncMonitor createVSyncMonitor(
            final VSyncMonitor.Listener listener, final boolean enableJBVSync) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<VSyncMonitor>() {
            @Override
            public VSyncMonitor call() {
                Context context = getInstrumentation().getContext();
                return new VSyncMonitor(context, listener, enableJBVSync);
            }
        });
    }

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    private void performVSyncPeriodTest(boolean enableJBVSync) throws InterruptedException {
        // Collect roughly one second of data on a 60 fps display.
        final int framesPerSecond = 60;
        final int secondsToRun = 1;
        final int totalFrames = secondsToRun * framesPerSecond;
        VSyncDataCollector collector = new VSyncDataCollector(totalFrames);
        VSyncMonitor monitor = createVSyncMonitor(collector, enableJBVSync);

        long reportedFramePeriod = monitor.getVSyncPeriodInMicroseconds();
        assertTrue(reportedFramePeriod > 0);

        assertFalse(collector.isDone());

        monitor.requestUpdate();
        collector.waitTillDone();
        assertTrue(collector.isDone());
        monitor.stop();

        // Check that the median frame rate is within 10% of the reported frame period.
        assertTrue(collector.mFrameCount == totalFrames);
        Arrays.sort(collector.mFramePeriods, 0, collector.mFramePeriods.length);
        long medianFramePeriod = collector.mFramePeriods[collector.mFramePeriods.length / 2];
        if (Math.abs(medianFramePeriod - reportedFramePeriod) > reportedFramePeriod * .1) {
            fail("Measured median frame period " + medianFramePeriod +
                 " differs by more than 10% from the reported frame period " +
                 reportedFramePeriod);
        }
    }

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    @MediumTest
    public void testVSyncPeriodAllowJBVSync() throws InterruptedException {
        performVSyncPeriodTest(true);
    }

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    @MediumTest
    public void testVSyncPeriodDisallowJBVSync() throws InterruptedException {
        performVSyncPeriodTest(false);
    }
}
