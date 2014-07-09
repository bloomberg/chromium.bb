// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.VSyncMonitor;

import java.util.Arrays;
import java.util.concurrent.Callable;

public class VSyncMonitorTest extends InstrumentationTestCase {
    private static class VSyncDataCollector implements VSyncMonitor.Listener {
        public long mFramePeriods[];
        public int mFrameCount;
        public long mLastVSyncCpuTimeMillis;

        private boolean mDone;
        private long mPreviousVSyncTimeMicros;
        private Object mSyncRoot = new Object();

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
            mLastVSyncCpuTimeMillis = SystemClock.uptimeMillis();
            if (mPreviousVSyncTimeMicros == 0) {
                mPreviousVSyncTimeMicros = vsyncTimeMicros;
            }
            else {
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
        collectAndCheckVSync(enableJBVSync, 60);
    }

    private void collectAndCheckVSync(
            boolean enableJBVSync, final int totalFrames)
            throws InterruptedException {
        VSyncDataCollector collector = new VSyncDataCollector(totalFrames);
        VSyncMonitor monitor = createVSyncMonitor(collector, enableJBVSync);

        long reportedFramePeriod = monitor.getVSyncPeriodInMicroseconds();
        assertTrue(reportedFramePeriod > 0);

        assertFalse(collector.isDone());
        monitor.requestUpdate();
        collector.waitTillDone();
        assertTrue(collector.isDone());

        // Check that the median frame rate is within 10% of the reported frame period.
        assertTrue(collector.mFrameCount == totalFrames - 1);
        Arrays.sort(collector.mFramePeriods, 0, collector.mFramePeriods.length);
        long medianFramePeriod = collector.mFramePeriods[collector.mFramePeriods.length / 2];
        if (Math.abs(medianFramePeriod - reportedFramePeriod) > reportedFramePeriod * .1) {
            fail("Measured median frame period " + medianFramePeriod
                    + " differs by more than 10% from the reported frame period "
                    + reportedFramePeriod + " for requested frames");
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

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    private void performVSyncActivationFromIdle(boolean enableJBVSync) throws InterruptedException {
        VSyncDataCollector collector = new VSyncDataCollector(1);
        VSyncMonitor monitor = createVSyncMonitor(collector, enableJBVSync);

        monitor.requestUpdate();
        collector.waitTillDone();
        assertTrue(collector.isDone());

        long period = monitor.getVSyncPeriodInMicroseconds() / 1000;
        long delay = SystemClock.uptimeMillis() - collector.mLastVSyncCpuTimeMillis;

        // The VSync should have activated immediately instead of at the next real vsync.
        assertTrue(delay < period);
    }

    @MediumTest
    public void testVSyncActivationFromIdleAllowJBVSync() throws InterruptedException {
        performVSyncActivationFromIdle(true);
    }

    @MediumTest
    public void testVSyncActivationFromIdleDisallowJBVSync() throws InterruptedException {
        performVSyncActivationFromIdle(false);
    }
}
