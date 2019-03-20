// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for EventTracker.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventTrackerTest {
    private EventTracker mEventTracker;

    @Mock
    private UsageStatsBridge mBridge;
    @Captor
    private ArgumentCaptor<
            Callback<List<org.chromium.chrome.browser.usage_stats.WebsiteEventProtos.WebsiteEvent>>>
            mLoadCallbackCaptor;
    @Captor
    private ArgumentCaptor<Callback<Boolean>> mWriteCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mEventTracker = new EventTracker(mBridge);
        verify(mBridge, times(1)).getAllEvents(mLoadCallbackCaptor.capture());
    }

    @Test
    public void testRangeQueries() {
        resolveLoadCallback();
        addEntries(100, 2l, 1l);
        mEventTracker.queryWebsiteEvents(0l, 50l).then(
                (result) -> { assertEquals(result.size(), 25); });
        mEventTracker.queryWebsiteEvents(0l, 49l).then(
                (result) -> { assertEquals(result.size(), 24); });
        mEventTracker.queryWebsiteEvents(0l, 51l).then(
                (result) -> { assertEquals(result.size(), 25); });
        mEventTracker.queryWebsiteEvents(0l, 1000l).then(
                (result) -> { assertEquals(result.size(), 100); });
        mEventTracker.queryWebsiteEvents(1l, 99l).then(
                (result) -> { assertEquals(result.size(), 49); });
    }

    private void addEntries(int quantity, long stepSize, long startTime) {
        for (int i = 0; i < quantity; i++) {
            Promise<Void> writePromise = mEventTracker.addWebsiteEvent(
                    new WebsiteEvent(startTime, "", WebsiteEvent.EventType.START));
            verify(mBridge, times(i + 1)).addEvents(any(), mWriteCallbackCaptor.capture());
            resolveWriteCallback();
            startTime += stepSize;
        }
    }

    private void resolveLoadCallback() {
        mLoadCallbackCaptor.getValue().onResult(new ArrayList<>());
    }

    private void resolveWriteCallback() {
        mWriteCallbackCaptor.getValue().onResult(true);
    }
}
