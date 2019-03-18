// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/**
 * Unit tests for EventTracker.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventTrackerTest {
    private EventTracker mEventTracker;

    @Before
    public void setUp() {
        mEventTracker = new EventTracker();
    }

    @Test
    public void testRangeQueries() {
        addEntries(100, 1l, 0l);
        List<WebsiteEvent> result = mEventTracker.queryWebsiteEvents(0l, 50l);
        assertEquals(result.size(), 50);

        result = mEventTracker.queryWebsiteEvents(0l, 100l);
        assertEquals(result.size(), 100);
    }

    private void addEntries(int quantity, long stepSize, long startTime) {
        for (int i = 0; i < quantity; i++) {
            mEventTracker.addWebsiteEvent(
                    new WebsiteEvent(startTime, "", WebsiteEvent.EventType.START));
            startTime += stepSize;
        }
    }
}
