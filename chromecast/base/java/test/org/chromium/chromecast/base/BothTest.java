// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for Both.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class BothTest {
    @Test
    public void testAccessMembersOfBoth() {
        Both<Integer, Boolean> x = Both.both(10, true);
        assertEquals((int) x.first, 10);
        assertEquals((boolean) x.second, true);
    }

    @Test
    public void testDeeplyNestedBothType() {
        // Yes you can do this.
        Both<Both<Both<String, String>, String>, String> x =
                Both.both(Both.both(Both.both("A", "B"), "C"), "D");
        assertEquals(x.first.first.first, "A");
        assertEquals(x.first.first.second, "B");
        assertEquals(x.first.second, "C");
        assertEquals(x.second, "D");
    }

    @Test
    public void testBothToString() {
        Both<String, String> x = Both.both("a", "b");
        assertEquals(x.toString(), "a, b");
    }

    @Test
    public void testUseGetFirstAsMethodReference() {
        Both<Integer, String> x = Both.both(1, "one");
        Function<Both<Integer, String>, Integer> getFirst = Both::getFirst;
        assertEquals((int) getFirst.apply(x), 1);
    }

    @Test
    public void testUseGetSecondAsMethodReference() {
        Both<Integer, String> x = Both.both(2, "two");
        Function<Both<Integer, String>, String> getSecond = Both::getSecond;
        assertEquals(getSecond.apply(x), "two");
    }
}
