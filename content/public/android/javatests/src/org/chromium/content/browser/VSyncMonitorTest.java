// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import java.util.Arrays;
import java.util.concurrent.Callable;

import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.VSyncMonitor;

public class VSyncMonitorTest extends InstrumentationTestCase {
    private class VSyncDataCollector implements VSyncMonitor.Listener {
        public long framePeriods[];
        public int frameCount;

        private long previousVSyncTimeMicros;

        VSyncDataCollector(int frames) {
            framePeriods = new long[frames];
        }

        @Override
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
            if (previousVSyncTimeMicros == 0) {
                previousVSyncTimeMicros = vsyncTimeMicros;
                return;
            }
            if (frameCount >= framePeriods.length) {
                synchronized (this) {
                    notify();
                }
                return;
            }
            framePeriods[frameCount++] = vsyncTimeMicros - previousVSyncTimeMicros;
            previousVSyncTimeMicros = vsyncTimeMicros;
            monitor.requestUpdate();
        }
    };

    // The vsync monitor must be created on the UI thread to avoid associating the underlying
    // Choreographer with the Looper from the test runner thread.
    private VSyncMonitor createVSyncMonitor(final VSyncMonitor.Listener listener) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<VSyncMonitor>() {
            @Override
            public VSyncMonitor call() {
                Context context = getInstrumentation().getContext();
                View dummyView = new View(context);
                return new VSyncMonitor(context, dummyView, listener);
            }
        });
    }

    // Check that the vsync period roughly matches the timestamps that the monitor generates.
    @MediumTest
    public void testVSyncPeriod() throws InterruptedException {
        // Collect roughly two seconds of data on a 60 fps display.
        VSyncDataCollector collector = new VSyncDataCollector(60 * 2);
        VSyncMonitor monitor = createVSyncMonitor(collector);

        long reportedFramePeriod = monitor.getVSyncPeriodInMicroseconds();
        assertTrue(reportedFramePeriod > 0);

        if (!monitor.isVSyncSignalAvailable()) {
            // Cannot do timing test without real vsync signal. We can still verify that the monitor
            // throws if we try to use it.
            try {
                monitor.requestUpdate();
                fail("IllegalStateException not thrown.");
            } catch (IllegalStateException e) {
            }
            return;
        }

        monitor.requestUpdate();
        synchronized (collector) {
            collector.wait();
        }
        monitor.stop();

        // Check that the median frame rate is within 10% of the reported frame period.
        Arrays.sort(collector.framePeriods, 0, collector.framePeriods.length);
        long medianFramePeriod = collector.framePeriods[collector.framePeriods.length / 2];
        if (Math.abs(medianFramePeriod - reportedFramePeriod) > reportedFramePeriod * .1) {
            fail("Measured median frame period " + medianFramePeriod +
                 " differs by more than 10% from the reported frame period " +
                 reportedFramePeriod);
        }
    }

    private class TestView extends View {
        private Runnable mPendingAction;

        public TestView(Context context) {
            super(context);
        }

        @Override
        public boolean postDelayed(Runnable action, long delayMillis) {
            mPendingAction = action;
            return true;
        }

        public void runPendingAction() {
            assertNotNull(mPendingAction);
            mPendingAction.run();
        }
    };

    private class TestTimeSource implements VSyncMonitor.TestTimeSource {
        long mCurrentTimeMillis;

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        void setCurrentTimeMillis(long currentTimeMillis) {
            mCurrentTimeMillis = currentTimeMillis;
        }
    };

    // Check the the vsync monitor terminates after we have stopped requesting updates for some
    // time.
    @MediumTest
    public void testVSyncTimeout() throws InterruptedException {
        VSyncMonitor.Listener listener = new VSyncMonitor.Listener() {
            @Override
            public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
                // No-op vsync listener.
            }
        };
        VSyncMonitor monitor = createVSyncMonitor(listener);
        if (!monitor.isVSyncSignalAvailable()) {
            // Cannot do timeout test without real vsync signal.
            return;
        }

        TestTimeSource testTimeSource = new TestTimeSource();
        TestView testView = new TestView(getInstrumentation().getContext());
        monitor.setTestDependencies(testView, testTimeSource);

        // Requesting an update should make the monitor active.
        testTimeSource.setCurrentTimeMillis(0);
        monitor.requestUpdate();
        assertTrue(monitor.isActive());

        // Running the timeout callback should have no effect when no time has passed.
        testView.runPendingAction();
        assertTrue(monitor.isActive());

        // Ditto after half the timeout interval.
        testTimeSource.setCurrentTimeMillis(VSyncMonitor.VSYNC_TIMEOUT_MILLISECONDS / 2);
        testView.runPendingAction();
        assertTrue(monitor.isActive());

        // The monitor should be stopped after the timeout interval has passed.
        testTimeSource.setCurrentTimeMillis(VSyncMonitor.VSYNC_TIMEOUT_MILLISECONDS + 1);
        testView.runPendingAction();
        assertFalse(monitor.isActive());
    }
}
