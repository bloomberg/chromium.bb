// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v7.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for the TouchlessNewTabPageFocus class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TouchlessNewTabPageFocusInfoTest {
    @Test
    public void roundTripSimple() {
        TouchlessNewTabPageFocusInfo original = new TouchlessNewTabPageFocusInfo(
                TouchlessNewTabPageFocusInfo.FocusType.ARTICLE, 34);
        TouchlessNewTabPageFocusInfo roundTripped =
                TouchlessNewTabPageFocusInfo.deserialize(original.serialize());
        Assert.assertEquals(original.type, roundTripped.type);
        Assert.assertEquals(original.index, roundTripped.index);
    }

    @Test
    public void roundTripNegative() {
        TouchlessNewTabPageFocusInfo original = new TouchlessNewTabPageFocusInfo(
                TouchlessNewTabPageFocusInfo.FocusType.ARTICLE, -34);
        TouchlessNewTabPageFocusInfo roundTripped =
                TouchlessNewTabPageFocusInfo.deserialize(original.serialize());
        Assert.assertEquals(original.type, roundTripped.type);
        Assert.assertEquals(original.index, roundTripped.index);
    }

    @Test
    public void roundTripZero() {
        TouchlessNewTabPageFocusInfo original =
                new TouchlessNewTabPageFocusInfo(TouchlessNewTabPageFocusInfo.FocusType.ARTICLE, 0);
        TouchlessNewTabPageFocusInfo roundTripped =
                TouchlessNewTabPageFocusInfo.deserialize(original.serialize());
        Assert.assertEquals(original.type, roundTripped.type);
        Assert.assertEquals(original.index, roundTripped.index);
    }

    @Test
    public void roundTripNoIndex() {
        TouchlessNewTabPageFocusInfo original =
                new TouchlessNewTabPageFocusInfo(TouchlessNewTabPageFocusInfo.FocusType.LAST_TAB);
        TouchlessNewTabPageFocusInfo roundTripped =
                TouchlessNewTabPageFocusInfo.deserialize(original.serialize());
        Assert.assertEquals(original.type, roundTripped.type);
        Assert.assertEquals(original.index, roundTripped.index);
    }

    @Test
    public void deserializeFailure() {
        TouchlessNewTabPageFocusInfo position = TouchlessNewTabPageFocusInfo.deserialize("invalid");
        Assert.assertEquals(TouchlessNewTabPageFocusInfo.FocusType.UNKNOWN, position.type);
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
    }

    @Test
    public void deserializeEmpty() {
        TouchlessNewTabPageFocusInfo position = TouchlessNewTabPageFocusInfo.deserialize("");
        Assert.assertEquals(TouchlessNewTabPageFocusInfo.FocusType.UNKNOWN, position.type);
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
    }

    @Test
    public void deserializeNull() {
        TouchlessNewTabPageFocusInfo position = TouchlessNewTabPageFocusInfo.deserialize(null);
        Assert.assertEquals(TouchlessNewTabPageFocusInfo.FocusType.UNKNOWN, position.type);
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
    }
}