// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.NativeLibraryTestRule;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests of {@link PrecacheUMA}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PrecacheUMATest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Test
    @SmallTest
    @Feature({"Precache"})
    public void testUMAEventBitPositionAndMask() {
        // Test the bitmask and bit position of all UMA Events.
        for (int event = PrecacheUMA.Event.EVENT_START; event < PrecacheUMA.Event.EVENT_END;
                ++event) {
            long bitmask = PrecacheUMA.Event.getBitMask(event);
            Assert.assertTrue(bitmask > 0);
            Assert.assertEquals(bitmask, 1L << PrecacheUMA.Event.getBitPosition(event));
        }
    }

    @Test
    @SmallTest
    @Feature({"Precache"})
    public void testEventsInBitMask() {
        int[] events = {PrecacheUMA.Event.PRECACHE_TASK_STARTED_PERIODIC,
                PrecacheUMA.Event.PRECACHE_CANCEL_NO_UNMETERED_NETWORK,
                PrecacheUMA.Event.PRECACHE_SESSION_STARTED,
                PrecacheUMA.Event.PRECACHE_SESSION_COMPLETE};

        long bitmask = 0;
        for (int event : events) {
            bitmask = PrecacheUMA.Event.addEventToBitMask(bitmask, event);
        }
        assert Arrays.equals(events, PrecacheUMA.Event.getEventsFromBitMask(bitmask));
    }

    @Test
    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testRecordUMA_NativeLibraryNotLoaded() {
        // Tests that events are saved in preferences when native library is not loaded.
        List<Integer> events = new ArrayList<>();
        events.add(PrecacheUMA.Event.PRECACHE_TASK_STARTED_PERIODIC);
        events.add(PrecacheUMA.Event.PRECACHE_CANCEL_NO_UNMETERED_NETWORK);
        events.add(PrecacheUMA.Event.PRECACHE_SESSION_STARTED);
        events.add(PrecacheUMA.Event.PRECACHE_SESSION_COMPLETE);

        long bitmask = 0;
        for (int event : events) {
            PrecacheUMA.record(event);
            bitmask = PrecacheUMA.Event.addEventToBitMask(bitmask, event);
        }
        Assert.assertEquals(false, LibraryLoader.isInitialized());
        Assert.assertEquals(bitmask,
                ContextUtils.getAppSharedPreferences().getLong(
                        PrecacheUMA.PREF_PERSISTENCE_METRICS, 0));

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
        Assert.assertEquals(true, LibraryLoader.isInitialized());

        // When the library is initialized the events saved in preferences are dumped to histograms.
        HistogramDelta histograms[] = new HistogramDelta[PrecacheUMA.Event.EVENT_END];
        for (int event = PrecacheUMA.Event.EVENT_START; event < PrecacheUMA.Event.EVENT_END;
                ++event) {
            histograms[PrecacheUMA.Event.getBitPosition(event)] = new HistogramDelta(
                    PrecacheUMA.EVENTS_HISTOGRAM, PrecacheUMA.Event.getBitPosition(event));
        }

        // The next event will trigger the recording of the UMA metric.
        PrecacheUMA.record(PrecacheUMA.Event.PERIODIC_TASK_SCHEDULE_UPGRADE);
        events.add(PrecacheUMA.Event.PERIODIC_TASK_SCHEDULE_UPGRADE);
        Assert.assertEquals(0,
                ContextUtils.getAppSharedPreferences().getLong(
                        PrecacheUMA.PREF_PERSISTENCE_METRICS, 0));

        for (int event = PrecacheUMA.Event.EVENT_START; event < PrecacheUMA.Event.EVENT_END;
                ++event) {
            if (events.contains(event)) {
                Assert.assertEquals(
                        1, histograms[PrecacheUMA.Event.getBitPosition(event)].getDelta());
            } else {
                Assert.assertEquals(
                        0, histograms[PrecacheUMA.Event.getBitPosition(event)].getDelta());
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testRecordUMA_NativeLibraryLoaded() {
        // Test that events are recorded as UMA metric when library is initialized.
        List<Integer> events = new ArrayList<>();
        events.add(PrecacheUMA.Event.PRECACHE_TASK_STARTED_PERIODIC);
        events.add(PrecacheUMA.Event.PRECACHE_CANCEL_NO_UNMETERED_NETWORK);
        events.add(PrecacheUMA.Event.PRECACHE_SESSION_STARTED);
        events.add(PrecacheUMA.Event.PRECACHE_SESSION_COMPLETE);

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
        Assert.assertEquals(true, LibraryLoader.isInitialized());

        HistogramDelta histograms[] = new HistogramDelta[PrecacheUMA.Event.EVENT_END];
        for (int event = PrecacheUMA.Event.EVENT_START; event < PrecacheUMA.Event.EVENT_END;
                ++event) {
            histograms[PrecacheUMA.Event.getBitPosition(event)] = new HistogramDelta(
                    PrecacheUMA.EVENTS_HISTOGRAM, PrecacheUMA.Event.getBitPosition(event));
        }
        for (int event : events) {
            PrecacheUMA.record(event);
            Assert.assertEquals(0,
                    ContextUtils.getAppSharedPreferences().getLong(
                            PrecacheUMA.PREF_PERSISTENCE_METRICS, 0));
        }
        for (int event = PrecacheUMA.Event.EVENT_START; event < PrecacheUMA.Event.EVENT_END;
                ++event) {
            if (events.contains(event)) {
                Assert.assertEquals(
                        1, histograms[PrecacheUMA.Event.getBitPosition(event)].getDelta());
            } else {
                Assert.assertEquals(
                        0, histograms[PrecacheUMA.Event.getBitPosition(event)].getDelta());
            }
        }
    }
}
